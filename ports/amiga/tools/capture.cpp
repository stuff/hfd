// Records an Amiga program in vAmiga, as Hatari's recording does for the ST.
//
//   capture <kickstart.rom> <disk.adf> <first frame> <frame count> <out dir>
//   capture --audio <kickstart.rom> <disk.adf> <first frame> <seconds> <out.wav>
//
// Boots an A500 (OCS, 512 KB chip + 512 KB slow) from the disk and runs to the first
// frame, as fast as possible. Frame n is VBL n after power-on, so two runs give the same
// result.
// - Frames: writes each frame as <out dir>/NNNNN.ppm, the whole PAL frame at lowres
//   resolution (456 × 313, one pixel per lowres pixel, VBL and HBL areas included), and
//   in <out dir>/vbl.txt the frame numbers with the VBL the program shows (see step()).
// - Audio: vAmiga mutes the sound in warp mode and while paused, so it then runs in VSYNC
//   mode, one frame per wakeup, and records Paula's output (44.1 kHz, 16-bit stereo, the
//   A500 filter, vAmiga's default stereo separation). The first 0.2 s fade in.
//
// CAPTURE_KEYS=frame:code,... presses keys (raw Amiga codes in hex, for 0.1 s) at the
// start of these frames.
// For debugging, with frames: CAPTURE_DEBUG=1 prints the CPU's registers 100 lines into
// each frame (out of the VBL interrupt), and CAPTURE_DUMP=frame,address,length,file (hex
// address and length; several, separated by ";") also saves that memory then.

#include "VAmiga.h"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <mutex>
#include <thread>

using namespace vamiga;

static std::mutex mutex;
static std::condition_variable cond;
static bool powered = false, eof = false, eol = false;

static void listener(const void *, Message msg)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (msg.type == Msg::POWER && msg.value) powered = true;
    if (msg.type == Msg::EOF_REACHED) eof = true;
    if (msg.type == Msg::EOL_REACHED) eol = true;
    cond.notify_all();
}

static void wait_for(bool &flag)
{
    std::unique_lock<std::mutex> lock(mutex);
    cond.wait(lock, [&] { return flag; });
    flag = false;
}

static void pause_after(VAmiga &amiga, void (VAmiga::*run)(), bool &flag)
{
    (amiga.*run)();
    amiga.wakeUp();  // otherwise the paused thread sleeps up to 50 ms before running
    wait_for(flag);
    while (!amiga.isPaused()) std::this_thread::yield();
}

// Runs one frame, then line 0 of the next one. The finished frame becomes the stable
// texture at the start of line 0, and the end-of-frame trap stops the CPU just before:
// usually before the swap, after it if an instruction (an interrupt) straddles it.
// Returns the word the program wrote at $1f0 during the frame: the VBL it shows.
static u16 step(VAmiga &amiga)
{
    pause_after(amiga, &VAmiga::finishFrame, eof);
    u16 shown = amiga.mem.debugger.spypeek16(Accessor::CPU, 0x1f0);
    pause_after(amiga, &VAmiga::finishLine, eol);
    return shown;
}

static std::map<long, int> keys_to_press()
{
    // CAPTURE_KEYS=frame:code,...: press a key (raw Amiga code, hex) at the start of a frame.
    std::map<long, int> keys;
    if (const char *list = getenv("CAPTURE_KEYS")) {
        std::string all = list;
        for (size_t at = 0; at < all.size();) {
            size_t end = all.find(',', at);
            std::string item = all.substr(at, end == std::string::npos ? std::string::npos : end - at);
            size_t colon = item.find(':');
            keys[std::stol(item.substr(0, colon))] = std::stoi(item.substr(colon + 1), nullptr, 16);
            if (end == std::string::npos) break;
            at = end + 1;
        }
    }
    return keys;
}

static void boot(VAmiga &amiga, const char *rom, const char *adf)
{
    amiga.launch(&amiga, listener);
    amiga.set(ConfigScheme::A500_OCS_1MB);
    amiga.set(Opt::MON_PALETTE, (i64)Palette::RGB);  // plain colours, as vAmiga's regression tests use
    amiga.set(Opt::DENISE_FRAME_SKIPPING, 0);         // in warp mode, Denise only renders 1 frame in 17
    amiga.set(Opt::AUD_ASR, 0);                       // exactly 44.1 kHz
    amiga.mem.loadRom(rom);
    amiga.df0.insert(adf, true);
    amiga.powerOn();
    wait_for(powered);
    while (amiga.isRunning()) std::this_thread::yield();
    amiga.warpOn(1);  // source 0 is reserved for the warp mode option
}

static int frames(VAmiga &amiga, long first, long count, const std::filesystem::path &out)
{
    std::filesystem::create_directories(out);
    const isize W = 456, H = 313;
    std::vector<unsigned char> rgb(W * H * 3);
    FILE *vbl = std::fopen((out / "vbl.txt").string().c_str(), "w");
    auto keys = keys_to_press();
    for (long frame = 0; frame < first + count; frame++) {
        if (auto key = keys.find(frame); key != keys.end()) {
            amiga.keyboard.press(KeyCode(key->second), 0.0, 0.1);
        }
        u16 shown = step(amiga);
        if (frame < first) continue;
        std::fprintf(vbl, "%ld %d\n", frame, shown);
        if (getenv("CAPTURE_DEBUG")) {
            // 100 lines later: out of the VBL interrupt, in the main loop.
            for (int i = 0; i < 100; i++) pause_after(amiga, &VAmiga::finishLine, eol);
            auto &cpu = amiga.cpu.getInfo();
            if (const char *dumps = getenv("CAPTURE_DUMP")) {  // frame,address,length,file;...
                for (const char *dump = dumps; dump; dump = std::strchr(dump, ';') ? std::strchr(dump, ';') + 1 : nullptr) {
                    long at; unsigned addr, len; char file[256];
                    if (std::sscanf(dump, "%ld,%x,%x,%255[^;]", &at, &addr, &len, file) == 4 && at == frame) {
                        FILE *d = std::fopen(file, "wb");
                        for (unsigned i = 0; i < len; i++) std::fputc(amiga.mem.debugger.spypeek8(Accessor::CPU, addr + i), d);
                        std::fclose(d);
                    }
                }
            }
            std::fprintf(stderr, "frame %ld pc %06x a3 %06x a4 %06x a5 %06x d0 %08x d1 %08x d2 %08x d7 %08x\n", frame, cpu.pc0, cpu.a[3], cpu.a[4], cpu.a[5], cpu.d[0], cpu.d[1], cpu.d[2], cpu.d[7]);
        }

        amiga.videoPort.lockTexture();
        isize nr; bool lof, prevlof;
        const u32 *tex = amiga.videoPort.getTexture(&nr, &lof, &prevlof);
        if (nr != frame) {
            std::cerr << "frame " << frame << ": got texture " << nr << "\n";
            return 1;
        }
        // The texture has one u32 per super-hires pixel (1824 per line): keep one in four.
        for (isize y = 0; y < H; y++) {
            for (isize x = 0; x < W; x++) {
                u32 t = tex[y * 1824 + 4 * x];
                unsigned char *p = &rgb[(y * W + x) * 3];
                p[0] = t & 0xff; p[1] = (t >> 8) & 0xff; p[2] = (t >> 16) & 0xff;
            }
        }
        amiga.videoPort.unlockTexture();

        char name[32];
        std::snprintf(name, sizeof name, "%05ld.ppm", frame);
        FILE *f = std::fopen((out / name).string().c_str(), "wb");
        std::fprintf(f, "P6\n%ld %ld\n255\n", (long)W, (long)H);
        std::fwrite(rgb.data(), 1, rgb.size(), f);
        std::fclose(f);
    }
    std::fclose(vbl);
    return 0;
}

static int audio(VAmiga &amiga, long first, double seconds, const char *out)
{
    auto keys = keys_to_press();
    for (long frame = 0; frame < first; frame++) {
        if (auto key = keys.find(frame); key != keys.end()) amiga.keyboard.press(KeyCode(key->second), 0.0, 0.1);
        step(amiga);
    }

    // From here on, one frame per wakeUp(), at whatever pace we drain the samples. The
    // audio buffer must never run dry: vAmiga then clears it and changes the sample rate.
    const isize cap = 65536;
    std::vector<float> left(cap), right(cap);
    auto available = [&] { return isize(amiga.audioPort.getStats().fillLevel * cap + 0.5); };
    amiga.set(Opt::AUD_BUFFER_SIZE, cap);
    amiga.set(Opt::AMIGA_VSYNC, 1);
    amiga.warpOff(1);
    amiga.run();
    // Leaving warp mode clears the buffer, leaving half of it as silence before the first
    // frame's samples.
    while (!amiga.isRunning()) std::this_thread::yield();
    amiga.audioPort.copyStereo(left.data(), right.data(), cap / 2);

    // vAmiga fades the sound in over its first 10000 samples.
    const isize rate = 44100, total = isize(seconds * rate);
    std::vector<int16_t> pcm;
    for (long frame = first; isize(pcm.size()) < 2 * total; frame++) {
        if (auto key = keys.find(frame); key != keys.end()) amiga.keyboard.press(KeyCode(key->second), 0.0, 0.1);
        amiga.wakeUp();
        while (available() < 880) std::this_thread::yield();  // a frame is 882.5 samples
        isize n = amiga.audioPort.copyStereo(left.data(), right.data(), available());
        for (isize i = 0; i < n; i++) {
            pcm.push_back(int16_t(std::clamp(left[i], -1.0f, 1.0f) * 32767));
            pcm.push_back(int16_t(std::clamp(right[i], -1.0f, 1.0f) * 32767));
        }
    }
    pcm.resize(2 * total);
    if (amiga.audioPort.getStats().bufferUnderflows || amiga.audioPort.getStats().bufferOverflows) {
        std::cerr << "audio buffer under- or overflow\n";
    }

    FILE *f = std::fopen(out, "wb");
    auto u32le = [&](u32 v) { std::fwrite(&v, 4, 1, f); };
    auto u16le = [&](u16 v) { std::fwrite(&v, 2, 1, f); };
    u32 bytes = u32(pcm.size() * 2);
    std::fwrite("RIFF", 1, 4, f); u32le(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32le(16); u16le(1); u16le(2); u32le(rate); u32le(rate * 4); u16le(4); u16le(16);
    std::fwrite("data", 1, 4, f); u32le(bytes);
    std::fwrite(pcm.data(), 2, pcm.size(), f);
    std::fclose(f);
    return 0;
}

int main(int argc, char *argv[])
{
    bool sound = argc == 7 && std::strcmp(argv[1], "--audio") == 0;
    if (argc != 6 && !sound) {
        std::cerr << "usage: capture <kickstart.rom> <disk.adf> <first frame> <frame count> <out dir>\n"
                     "       capture --audio <kickstart.rom> <disk.adf> <first frame> <seconds> <out.wav>\n";
        return 1;
    }
    char **arg = argv + (sound ? 2 : 1);

    VAmiga amiga;
    boot(amiga, arg[0], arg[1]);
    int result = sound ? audio(amiga, std::stol(arg[2]), std::stod(arg[3]), arg[4])
                       : frames(amiga, std::stol(arg[2]), std::stol(arg[3]), arg[4]);
    amiga.removeListener();
    std::_Exit(result);
}
