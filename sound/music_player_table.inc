	.ifdef PLATFORM_64BIT
	.equiv TRACK_SIZE, 0x78
	.else
	.equiv TRACK_SIZE, 0x50
	.endif
	.equiv NUM_TRACKS_BGM, 10
	.equiv NUM_TRACKS_SE1, 3
	.equiv NUM_TRACKS_SE2, 9
	.equiv NUM_TRACKS_SE3, 1

    .bss
    runtime_ptr_align

gMPlayTrack_BGM::
    .space TRACK_SIZE * NUM_TRACKS_BGM
    .size gMPlayTrack_BGM, .-gMPlayTrack_BGM

gMPlayTrack_SE1::
    .space TRACK_SIZE * NUM_TRACKS_SE1
    .size gMPlayTrack_SE1, .-gMPlayTrack_SE1

gMPlayTrack_SE2::
    .space TRACK_SIZE * NUM_TRACKS_SE2
    .size gMPlayTrack_SE2, .-gMPlayTrack_SE2

gMPlayTrack_SE3::
    .space TRACK_SIZE * NUM_TRACKS_SE3
    .size gMPlayTrack_SE3, .-gMPlayTrack_SE3

	.section .rodata

	asset_ptr_align
gMPlayTable::
	music_player gMPlayInfo_BGM, gMPlayTrack_BGM, NUM_TRACKS_BGM, 0
	music_player gMPlayInfo_SE1, gMPlayTrack_SE1, NUM_TRACKS_SE1, 1
	music_player gMPlayInfo_SE2, gMPlayTrack_SE2, NUM_TRACKS_SE2, 1
	music_player gMPlayInfo_SE3, gMPlayTrack_SE3, NUM_TRACKS_SE3, 0
