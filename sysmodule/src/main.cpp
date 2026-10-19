/**
 * CloudRedir Sysmodule Main Entry Point
 *
 * This sysmodule runs in the background and monitors game state changes.
 * When a game exits, it copies the save data to a temporary location and
 * initiates a sync to the configured cloud provider (e.g., Google Drive).
 */

#include <switch.h>
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <unordered_map>

// Forward declarations
void titleMonitorThread();
void syncThread();
bool copySaveData(u64 title_id);
bool uploadToCloud(const char* save_path, const char* remote_path);

// Global state
std::atomic<bool> g_running{true};
std::mutex g_save_mutex;
std::condition_variable g_save_cv;
std::vector<u64> g_titles_to_sync; // Titles whose save data is ready to sync

// Configuration (would be loaded from a shared config with the frontend app)
struct Config {
    char cloud_provider[64]; // e.g., "googledrive"
    char access_token[256];  // OAuth token (stored securely, but for simplicity we keep it here)
    char sync_folder[256];   // Remote folder on cloud service
} g_config;

// Initialize the sysmodule
int main(int argc, char* argv[]) {
    // Initialize services and services we need
    socketInitializeDefault();
    nsInitialize(); // For title notifications
    pmInitialize(); // For getting list of running titles (if needed)
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Load configuration from shared memory or storage (simplified)
    // In reality, we would use a shared memory block set by the frontend app.
    strcpy(g_config.cloud_provider, "googledrive");
    strcpy(g_config.access_token, "YOUR_ACCESS_TOKEN_HERE"); // Should be obtained via OAuth flow
    strcpy(g_config.sync_folder, "/CloudRedir/saves");

    // Start background threads
    std::thread monitor_thread(titleMonitorThread);
    std::thread sync_thread(syncThread);

    // Main thread waits for termination signal (e.g., from homebrew menu)
    while (g_running) {
        svcSleepThread(1000000000LL); // Sleep 1 second
    }

    // Cleanup
    g_running = false;
    monitor_thread.join();
    sync_thread.join();

    curl_global_cleanup();
    pmExit();
    nsExit();
    socketExit();

    return 0;
}

/**
 * Monitors the running titles and detects when a title exits.
 * We use ns:GetApplicationList periodically and compare with previous list.
 * When a title disappears from the list, we assume it has exited and queue its save data for sync.
 */
void titleMonitorThread() {
    std::unordered_map<u64, bool> last_running_titles;

    while (g_running) {
        // Get current list of running titles
        Result rc;
        u64 title_ids[0x40]; // Buffer for up to 64 titles
        size_t title_count = 0;

        rc = nsGetApplicationList(title_ids, 0x40, &title_count);
        if (R_FAILED(rc)) {
            // Handle error, maybe log and retry
            svcSleepThread(500000000LL); // Sleep 500ms
            continue;
        }

        // Determine which titles have exited
        std::vector<u64> exited_titles;
        for (size_t i = 0; i < title_count; ++i) {
            u64 tid = title_ids[i];
            if (last_running_titles.find(tid) == last_running_titles.end()) {
                // Newly appeared title (just started) - we don't care for saving on start
                last_running_titles[tid] = true;
            }
        }

        // Check for titles that were running but are no longer in the list
        for (auto it = last_running_titles.begin(); it != last_running_titles.end(); ) {
            u64 tid = it->first;
            bool still_running = false;
            for (size_t i = 0; i < title_count; ++i) {
                if (title_ids[i] == tid) {
                    still_running = true;
                    break;
                }
            }
            if (!still_running) {
                // Title has exited
                exited_titles.push_back(tid);
                it = last_running_titles.erase(it);
            } else {
                ++it;
            }
        }

        // For each exited title, copy its save data and queue for sync
        for (u64 tid : exited_titles) {
            // Copy save data to a temporary location
            if (copySaveData(tid)) {
                std::lock_guard<std::mutex> lock(g_save_mutex);
                g_titles_to_sync.push_back(tid);
                g_save_cv.notify_one();
            }
        }

        // Sleep for a short period to avoid excessive polling
        svcSleepThread(2000000000LL); // 2 seconds
    }
}

/**
 * Copies the save data for a given title ID to a temporary directory.
 * @param title_id The title ID of the game whose save data to copy.
 * @return true if successful, false otherwise.
 */
bool copySaveData(u64 title_id) {
    // Save data location: sd:/<title_id>/save/
    // We need to construct the source and destination paths.
    char source_path[260];
    char dest_path[260];

    // We'll use a temporary directory in the sysmodule's storage (e.g., save data of the sysmodule itself)
    // For simplicity, we use a fixed path. In reality, we would use a per-title temporary directory.
    sprintf(source_path, "sd:/%016llx/save/", (unsigned long long)title_id);
    sprintf(dest_path, "sd:/sysmodules/cloudredir/tmp/%016llx/", (unsigned long long)title_id);

    // Create destination directory
    fsdevmkdir(dest_path);

    // Copy all files from source to destination (recursively)
    // We'll use a simple file copy function (not shown here for brevity)
    // In practice, we would use a filesystem utility or implement a recursive copy.

    // For now, we assume a function copyDirectoryExists that returns true on success.
    // bool success = copyDirectory(source_path, dest_path);
    // return success;

    // Placeholder: always return true for skeleton
    return true;
}

/**
 * Thread that processes the queue of titles to sync and uploads their save data to the cloud.
 */
void syncThread() {
    while (g_running) {
        std::unique_lock<std::mutex> lock(g_save_mutex);
        g_save_cv.wait(lock, [] { return !g_titles_to_sync.empty() || !g_running; });

        if (!g_running && g_titles_to_sync.empty()) {
            break;
        }

        // Take a title ID from the queue
        u64 title_id = g_titles_to_sync.front();
        g_titles_to_sync.erase(g_titles_to_sync.begin());
        lock.unlock();

        // Construct the temporary save data path
        char save_path[260];
        sprintf(save_path, "sd:/sysmodules/cloudredir/tmp/%016llx/", (unsigned long long)title_id);

        // Construct the remote path (e.g., /CloudRedir/saves/<title_id>/)
        char remote_path[260];
        sprintf(remote_path, "%s/%016llx/", g_config.sync_folder, (unsigned long long)title_id);

        // Upload the save data to the cloud
        if (!uploadToCloud(save_path, remote_path)) {
            // Handle upload failure (maybe retry later)
            // For now, we just log and continue
            printf("Failed to upload save data for title %016llx\n", (unsigned long long)title_id);
        }

        // Optionally, clean up the temporary save data after successful upload
        // fsdevrmdir_recursive(save_path); // Not shown
    }
}

/**
 * Uploads a directory of save data to the cloud provider.
 * This is a simplified placeholder. In reality, we would need to:
 *   - Traverse the directory and upload each file.
 *   - Handle the specific cloud provider's API (e.g., Google Drive REST API).
 *   - Use the access token for authentication.
 *   - Handle network errors and retries.
 *
 * @param save_path Local path to the save data directory.
 * @param remote_path Remote path on the cloud service where the save data should be stored.
 * @return true if successful, false otherwise.
 */
bool uploadToCloud(const char* save_path, const char* remote_path) {
    // Placeholder implementation: always returns true for the skeleton.
    // In a real implementation, we would use libcurl to perform HTTP requests.
    printf("Uploading save data from %s to %s (placeholder)\n", save_path, remote_path);
    return true;
}