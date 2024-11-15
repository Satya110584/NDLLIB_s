#include <iostream>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <memory>   // for std::unique_ptr
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>

#ifdef _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x64.lib")
#else // _WIN64
#pragma comment(lib, "Processing.NDI.Lib.x86.lib")
#endif // _WIN64

#endif

#include <Processing.NDI.Lib.h>

// RAII wrapper for NDI sender
class NDISender {
public:
    NDISender() {
        // Initialize the NDI library
        if (!NDIlib_initialize()) {
            throw std::runtime_error("Failed to initialize NDI library.");
        }

        // Create the NDI sender
        NDIlib_send_create_t NDI_send_create_desc;
        NDI_send_create_desc.p_ndi_name = "My Audio";
        NDI_send_create_desc.clock_audio = true;

        pNDI_send = NDIlib_send_create(&NDI_send_create_desc);
        if (!pNDI_send) {
            throw std::runtime_error("Failed to create NDI sender.");
        }
    }

    ~NDISender() {
        // Destroy the NDI sender and clean up
        if (pNDI_send) {
            NDIlib_send_destroy(pNDI_send);
        }
        NDIlib_destroy();  // Clean up NDI library
    }

    NDIlib_send_instance_t get() const {
        return pNDI_send;
    }

private:
    NDIlib_send_instance_t pNDI_send = nullptr;
};

// RAII wrapper for NDI audio frame
class NDIAudioFrame {
public:
    NDIAudioFrame(int no_channels, int no_samples, int sample_rate)
        : no_channels(no_channels), no_samples(no_samples), sample_rate(sample_rate) {

        // Allocate memory for the audio data (RAII)
        p_data = std::make_unique<float[]>(no_samples * no_channels);

        NDI_audio_frame.sample_rate = sample_rate;
        NDI_audio_frame.no_channels = no_channels;
        NDI_audio_frame.no_samples = no_samples;
        NDI_audio_frame.p_data = p_data.get();
        NDI_audio_frame.channel_stride_in_bytes = no_samples * sizeof(float);
    }

    NDIlib_audio_frame_v2_t* get() {
        return &NDI_audio_frame;
    }

    float* data() {
        return p_data.get();
    }

private:
    int no_channels;
    int no_samples;
    int sample_rate;
    NDIlib_audio_frame_v2_t NDI_audio_frame;
    std::unique_ptr<float[]> p_data; // RAII handles memory
};

static std::atomic<bool> exit_loop(false);
static void sigint_handler(int) { exit_loop = true; }

int main(int argc, char* argv[]) {
    try {
        // Catch interrupt to shut down gracefully
        signal(SIGINT, sigint_handler);

        // Create NDI sender (RAII)
        NDISender ndiSender;

        // Create an NDI audio frame (RAII)
        NDIAudioFrame audioFrame(4, 1920, 48000);

        // Send 1000 frames of audio
        for (int idx = 0; !exit_loop && idx < 1000; idx++) {
            // Fill the buffer with silence (or your own audio processing)
            for (int ch = 0; ch < audioFrame.get()->no_channels; ch++) {
                // Get the pointer to the start of this channel
                float* p_ch = (float*)((uint8_t*)audioFrame.get()->p_data + ch * audioFrame.get()->channel_stride_in_bytes);

                // Fill it with silence (for demonstration purposes)
                for (int sample_no = 0; sample_no < audioFrame.get()->no_samples; sample_no++) {
                    p_ch[sample_no] = 0.0f;
                }
            }

            // Submit the frame
            NDIlib_send_send_audio_v2(ndiSender.get(), audioFrame.get());

            // Display something helpful
            std::cout << "Frame number " << idx << " sent." << std::endl;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
