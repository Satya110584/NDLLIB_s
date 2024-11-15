#include <cstdio>
#include <chrono>
#include <Processing.NDI.Lib.h>

#ifdef _WIN32
#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64/Users/sraghuwanshi/Downloads/NDI SDK for Linux/examples/C++/NDIlib_Recv_Audio_16bpp/NDIlib_Recv_Audio_16bpp.cpp
#endif // _WIN32

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
        pNDI_find = NDIlib_find_create_v2();
        if (!pNDI_find) {
            throw std::runtime_error("Failed to create NDI finder");
        }
    }

    ~NDIFinder() {
        NDIlib_find_destroy(pNDI_find);
    }

    NDIlib_find_instance_t get() const {
        return pNDI_find;
    }

private:
    NDIlib_find_instance_t pNDI_find;
};

// RAII class for handling NDI receiver instance
class NDIReceiver {
public:
    NDIReceiver() {
        pNDI_recv = NDIlib_recv_create_v3();
        if (!pNDI_recv) {
            throw std::runtime_error("Failed to create NDI receiver");
        }
    }

    ~NDIReceiver() {
        NDIlib_recv_destroy(pNDI_recv);
    }

    void connect(const NDIlib_source_t& source) {
        NDIlib_recv_connect(pNDI_recv, &source);
    }

    int capture(NDIlib_video_frame_v2_t& video_frame, NDIlib_audio_frame_v2_t& audio_frame) {
        return NDIlib_recv_capture_v2(pNDI_recv, &video_frame, &audio_frame, nullptr, 5000);
    }

    void free_video(NDIlib_video_frame_v2_t& video_frame) {
        NDIlib_recv_free_video_v2(pNDI_recv, &video_frame);
    }

    void free_audio(NDIlib_audio_frame_v2_t& audio_frame) {
        NDIlib_recv_free_audio_v2(pNDI_recv, &audio_frame);
    }

private:
    NDIlib_recv_instance_t pNDI_recv;
};

int main(int argc, char* argv[])
{
    try {
        // Initialize NDI using RAII
        NDIInitializer ndiInit;

        // Create NDI finder using RAII
        NDIFinder ndiFinder;

        // Wait until there is at least one source
        uint32_t no_sources = 0;
        const NDIlib_source_t* p_sources = nullptr;
        while (!no_sources) {
            // Wait until the sources on the network have changed
            printf("Looking for sources ...\n");
            NDIlib_find_wait_for_sources(ndiFinder.get(), 1000 /* One second */);
            p_sources = NDIlib_find_get_current_sources(ndiFinder.get(), &no_sources);
        }

        // We now have at least one source, so we create a receiver to look at it using RAII
        NDIReceiver ndiReceiver;

        // Connect to the first source
        ndiReceiver.connect(p_sources[0]);

        // Destroy the NDI finder (already done automatically when ndiFinder goes out of scope)
        // Run for one minute
        using namespace std::chrono;
        for (const auto start = high_resolution_clock::now(); high_resolution_clock::now() - start < minutes(5);) {
            // The descriptors
            NDIlib_video_frame_v2_t video_frame;
            NDIlib_audio_frame_v2_t audio_frame;

            switch (ndiReceiver.capture(video_frame, audio_frame)) {
                case NDIlib_frame_type_none:
                    printf("No data received.\n");
                    break;

                case NDIlib_frame_type_video:
                    printf("Video data received (%dx%d).\n", video_frame.xres, video_frame.yres);
                    ndiReceiver.free_video(video_frame);
                    break;

                case NDIlib_frame_type_audio:
                    printf("Audio data received (%d samples).\n", audio_frame.no_samples);
                    ndiReceiver.free_audio(audio_frame);
                    break;
            }
        }

        // No explicit cleanup needed, RAII will take care of destroying resources
        return 0;
    }
    catch (const std::exception& e) {
        // Handle errors, print the message, and return a non-zero value
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
