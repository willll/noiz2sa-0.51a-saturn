/*
 * $Id: soundmanager.h,v 1.1.1.1 2002/11/03 11:08:24 kenta Exp $
 *
 * Copyright 2002 Kenta Cho. All rights reserved.
 */

/**
 * BGM/SE manager header file.
 *
 * @version $Revision: 1.1.1.1 $
 */
/** @brief Shuts down the audio subsystem and releases loaded sounds. */
void closeSound();
/** @brief Initialises the audio subsystem. */
void initSound();
/** @brief Loads BGM and sound effect assets from disc. */
void loadSounds();
/**
 * @brief Starts playing a background music track.
 * @param idx Track index.
 */
void playMusic(int idx);
/** @brief Fades out the currently playing music. */
void fadeMusic();
/** @brief Stops the currently playing music immediately. */
void stopMusic();
/**
 * @brief Plays a sound effect chunk.
 * @param idx Chunk index.
 */
void playChunk(int idx);
/** @brief Advances sound playback state for the current frame. */
void soundTick();
