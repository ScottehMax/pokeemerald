// Headless mGBA with its Dolphin (JOY Bus) link attached, for testing the
// viewer ROM against lvbridge.py without the Qt frontend. It does what
// "File > Connect to Dolphin..." does, runs a number of frames and saves
// screenshots. Build with tools/link_viewer/test/build_harness.sh.
//
//   mgba_harness ROM FRAMES SHOT_EVERY OUT_DIR [RESET_AT_FRAME]

#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/sio/dolphin.h>
#include <mgba-util/image/png-io.h>
#include <mgba-util/socket.h>
#include <mgba-util/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void Screenshot(struct mCore* core, const mColor* pixels, unsigned width, unsigned height, const char* path) {
	struct VFile* vf = VFileOpen(path, O_CREAT | O_TRUNC | O_WRONLY);
	if (!vf) {
		fprintf(stderr, "can't write %s\n", path);
		return;
	}
	png_structp png = PNGWriteOpen(vf);
	png_infop info = PNGWriteHeader(png, width, height, mCOLOR_NATIVE);
	PNGWritePixels(png, width, height, width, pixels, mCOLOR_NATIVE);
	PNGWriteClose(png, info);
	vf->close(vf);
	(void) core;
}

int main(int argc, char** argv) {
	if (argc < 5) {
		fprintf(stderr, "usage: %s ROM FRAMES SHOT_EVERY OUT_DIR [RESET_AT_FRAME]\n", argv[0]);
		return 1;
	}
	const char* romPath = argv[1];
	long frames = strtol(argv[2], NULL, 10);
	long shotEvery = strtol(argv[3], NULL, 10);
	const char* outDir = argv[4];
	long resetAt = argc > 5 ? strtol(argv[5], NULL, 10) : 0;

	SocketSubsystemInit();
	struct mCore* core = mCoreFind(romPath);
	if (!core || !core->init(core)) {
		fprintf(stderr, "can't load a core for %s\n", romPath);
		return 1;
	}
	mCoreInitConfig(core, NULL);

	unsigned width, height;
	core->baseVideoSize(core, &width, &height);
	mColor* pixels = calloc(width * height, sizeof(mColor));
	core->setVideoBuffer(core, pixels, width);

	if (!mCoreLoadFile(core, romPath)) {
		fprintf(stderr, "can't load %s\n", romPath);
		return 1;
	}
	core->reset(core);

	struct Address address = { .version = IPV4, .ipv4 = 0x7F000001 };
	struct GBASIODolphin dolphin;
	GBASIODolphinCreate(&dolphin);
	int tries;
	for (tries = 0; tries < 100 && !GBASIODolphinConnect(&dolphin, &address, 0, 0); ++tries) {
		usleep(100000);
	}
	if (tries == 100) {
		fprintf(stderr, "no Dolphin host to connect to\n");
		return 1;
	}
	core->setPeripheral(core, mPERIPH_GBA_LINK_PORT, &dolphin.d);
	fprintf(stderr, "connected to the bridge\n");

	char path[512];
	long frame;
	for (frame = 1; frame <= frames; ++frame) {
		if (frame == resetAt) {
			fprintf(stderr, "resetting the GBA at frame %ld\n", frame);
			core->reset(core);
		}
		core->runFrame(core);
		if (shotEvery > 0 && frame % shotEvery == 0) {
			snprintf(path, sizeof(path), "%s/shot_%06ld.png", outDir, frame);
			Screenshot(core, pixels, width, height, path);
		}
	}
	snprintf(path, sizeof(path), "%s/last.png", outDir);
	Screenshot(core, pixels, width, height, path);

	core->setPeripheral(core, mPERIPH_GBA_LINK_PORT, NULL);
	GBASIODolphinDestroy(&dolphin);
	core->unloadROM(core);
	core->deinit(core);
	free(pixels);
	return 0;
}
