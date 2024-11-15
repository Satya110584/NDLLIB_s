#include <cstdio>
#include <chrono>
#include <thread>
#include <Processing.NDI.Lib.h>

#ifdef _WIN32
#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64
#endif // _WIN32

// RAII class to initialize and clean up NDI
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
        pNDI_find = NDIlib_find_create_v2();
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
    NDIReceiver() {
        pNDI_recv = NDIlib_recv_create_v3();
        if (!pNDI_recv) {
            throw std::runtime_error("Failed to create NDI receiver instance");
        }
    }

    ~NDIReceiver() {
        NDIlib_recv_destroy(pNDI_recv);
    }

    void connect(const NDIlib_source_t* source) {
        NDIlib_recv_connect(pNDI_recv, source);
    }

    NDIlib_recv_instance_t get() const { return pNDI_recv; }

private:
    NDIlib_recv_instance_t pNDI_recv;
};

// RAII class for handling NDI frame sync
class NDIFrameSync {
public:
    NDIFrameSync(NDIlib_recv_instance_t pNDI_recv) {
        pNDI_framesync = NDIlib_framesync_create(pNDI_recv);
        if (!pNDI_framesync) {
            throw std::runtime_error("Failed to create NDI frame sync instance");
        }
    }

    ~NDIFrameSync() {
        NDIlib_framesync_destroy(pNDI_framesync);
    }

    void capture_video(NDIlib_video_frame_v2_t* video_frame) {
        NDIlib_framesync_capture_video(pNDI_framesync, video_frame);
    }

    void capture_audio(NDIlib_audio_frame_v2_t* audio_frame, int sample_rate, int channels, int samples) {
        NDIlib_framesync_capture_audio(pNDI_framesync, audio_frame, sample_rate, channels, samples);
    }

    void free_video(NDIlib_video_frame_v2_t* video_frame) {
        NDIlib_framesync_free_video(pNDI_framesync, video_frame);
    }

    void free_audio(NDIlib_audio_frame_v2_t* audio_frame) {
        NDIlib_framesync_free_audio(pNDI_framesync, audio_frame);
    }

private:
    NDIlib_framesync_instance_t pNDI_framesync;
};

int main(int argc, char* argv[])
{
    try {
        // Initialize NDI using RAII
        NDIInitializer ndiInit;

        // Create NDI finder instance using RAII
        NDIFinder ndiFinder;

        uint32_t no_sources = 0;
        const NDIlib_source_t* p_sources = nullptr;

        // Wait until we find at least one source
        while (!no_sources) {
            printf("Looking for sources...\n");
            NDIlib_find_wait_for_sources(ndiFinder.get(), 1000);  // Wait for 1 second
            p_sources = NDIlib_find_get_current_sources(ndiFinder.get(), &no_sources);
        }

        // We need at least one source to proceed
        if (!p_sources) {
            printf("No sources found.\n");
            return 0;
        }

        // Create NDI receiver instance and connect to the first source using RAII
        NDIReceiver ndiReceiver;
        ndiReceiver.connect(p_sources);

        // Create frame synchronizer using RAII
        NDIFrameSync ndiFrameSync(ndiReceiver.get());

        // Run for five minutes
        using namespace std::chrono;
        for (const auto start = high_resolution_clock::now(); high_resolution_clock::now() - start < minutes(5);) {
            NDIlib_video_frame_v2_t video_frame;
            ndiFrameSync.capture_video(&video_frame);

            // Display video here if necessary
            if (video_frame.p_data) {
                // Display the video frame.
            }

            // Free the video frame
            ndiFrameSync.free_video(&video_frame);

            NDIlib_audio_frame_v2_t audio_frame;
            ndiFrameSync.capture_audio(&audio_frame, 48000, 4, 1600);

            // Process or play audio here

            // Free the audio frame
            ndiFrameSync.free_audio(&audio_frame);

            // Maintain a 30Hz loop (approximately 33ms between iterations)
            std::this_thread::sleep_for(milliseconds(33));
        }

        // All resources are cleaned up automatically when RAII objects go out of scope
        return 0;

    } catch (const std::exception& e) {
        // Handle initialization or runtime failures
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
