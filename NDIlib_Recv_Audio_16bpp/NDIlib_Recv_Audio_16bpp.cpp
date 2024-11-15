#include <csignal>
#include <cstddef>
#include <cstdio>
#include <atomic>
#include <chrono>

#ifdef _WIN32
#include <windows.h>

#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64

#endif

#include <Processing.NDI.Lib.h>

static std::atomic<bool> exit_loop(false);

// Signal handler to handle graceful exit
static void sigint_handler(int) { exit_loop = true; }

// RAII class for initializing and cleaning up NDI
class NDIInitializer {
public:
    NDIInitializer() {
        if (!NDIlib_initialize()) {
            throw std::runtime_error("Failed to initialize NDI");
        }
    }

    ~NDIInitializer() {
        NDIlib_destroy();
    }
};

// RAII class for handling NDI find instance
class NDIFinder {
public:
    NDIFinder() {
        const NDIlib_find_create_t NDI_find_create_desc = {};  // Default settings
        pNDI_find = NDIlib_find_create_v2(&NDI_find_create_desc);
        if (!pNDI_find) {
            throw std::runtime_error("Failed to create NDI find instance");
        }
    }

    ~NDIFinder() {
        NDIlib_find_destroy(pNDI_find);
    }

    NDIlib_find_instance_t get() const { return pNDI_find; }

private:
    NDIlib_find_instance_t pNDI_find;
};

// RAII class for handling NDI receiver instance
class NDIReceiver {
public:
    NDIReceiver(const NDIlib_source_t& source) {
        NDIlib_recv_create_v3_t NDI_recv_create_desc = {};
        NDI_recv_create_desc.source_to_connect_to = source;
        NDI_recv_create_desc.p_ndi_recv_name = "Example Audio Converter Receiver";

        pNDI_recv = NDIlib_recv_create_v3(&NDI_recv_create_desc);
        if (!pNDI_recv) {
            throw std::runtime_error("Failed to create NDI receiver");
        }
    }

    ~NDIReceiver() {
        NDIlib_recv_destroy(pNDI_recv);
    }

    int capture(NDIlib_video_frame_v2_t& video_frame, NDIlib_audio_frame_v2_t& audio_frame, NDIlib_metadata_frame_t& metadata_frame) {
        return NDIlib_recv_capture_v2(pNDI_recv, &video_frame, &audio_frame, &metadata_frame, 1000);
    }

    void free_video(NDIlib_video_frame_v2_t& video_frame) {
        NDIlib_recv_free_video_v2(pNDI_recv, &video_frame);
    }

    void free_audio(NDIlib_audio_frame_v2_t& audio_frame) {
        NDIlib_recv_free_audio_v2(pNDI_recv, &audio_frame);
    }

    void free_metadata(NDIlib_metadata_frame_t& metadata_frame) {
        NDIlib_recv_free_metadata(pNDI_recv, &metadata_frame);
    }

private:
    NDIlib_recv_instance_t pNDI_recv;
};

int main(int argc, char* argv[])
{
    try {
        // Initialize NDI using RAII
        NDIInitializer ndiInit;

        // Catch interrupt signal to shut down gracefully
        signal(SIGINT, sigint_handler);

        // Create NDI finder instance using RAII
        NDIFinder ndiFinder;

        uint32_t no_sources = 0;
        const NDIlib_source_t* p_sources = nullptr;

        // Wait until we find at least one source
        while (!exit_loop && !no_sources) {
            printf("Looking for sources...\n");
            NDIlib_find_wait_for_sources(ndiFinder.get(), 1000);  // Wait for 1 second
            p_sources = NDIlib_find_get_current_sources(ndiFinder.get(), &no_sources);
        }

        // If no sources found, exit
        if (!p_sources) {
            printf("No sources found.\n");
            return 0;
        }

        // Create NDI receiver to connect to the first source using RAII
        NDIReceiver ndiReceiver(p_sources[0]);

        // Destroy the NDI finder since we no longer need it
        // (it will be destroyed automatically when ndiFinder goes out of scope)

        // Run for up to one minute
        const auto start = std::chrono::high_resolution_clock::now();
        while (!exit_loop && std::chrono::high_resolution_clock::now() - start < std::chrono::minutes(1)) {
            NDIlib_video_frame_v2_t video_frame;
            NDIlib_audio_frame_v2_t audio_frame;
            NDIlib_metadata_frame_t metadata_frame;

            switch (ndiReceiver.capture(video_frame, audio_frame, metadata_frame)) {
                case NDIlib_frame_type_none:
                    printf("No data received.\n");
                    break;

                case NDIlib_frame_type_video:
                    printf("Video data received (%dx%d).\n", video_frame.xres, video_frame.yres);
                    ndiReceiver.free_video(video_frame);
                    break;

                case NDIlib_frame_type_audio: {
                    printf("Audio data received (%d samples).\n", audio_frame.no_samples);

                    // Convert audio data to interleaved format
                    NDIlib_audio_frame_interleaved_16s_t audio_frame_16bpp_interleaved;
                    audio_frame_16bpp_interleaved.reference_level = 20;  // 20dB of headroom
                    audio_frame_16bpp_interleaved.p_data = new short[audio_frame.no_samples * audio_frame.no_channels];

                    NDIlib_util_audio_to_interleaved_16s_v2(&audio_frame, &audio_frame_16bpp_interleaved);

                    // Free original audio buffer
                    ndiReceiver.free_audio(audio_frame);

                    // Process the interleaved audio data (not shown here)

                    // Clean up the interleaved audio data
                    delete[] audio_frame_16bpp_interleaved.p_data;
                    break;
                }

                case NDIlib_frame_type_metadata:
                    printf("Meta data received.\n");
                    ndiReceiver.free_metadata(metadata_frame);
                    break;

                case NDIlib_frame_type_status_change:
                    printf("Receiver connection status changed.\n");
                    break;

                default:
                    break;
            }
        }

        // Finished, all resources are automatically cleaned up when RAII objects go out of scope
        return 0;
    }
    catch (const std::exception& e) {
        // Handle initialization failure or any other errors
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
