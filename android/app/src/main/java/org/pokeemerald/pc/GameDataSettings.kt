package org.pokeemerald.pc

import android.content.Context
import java.io.File
import java.io.IOException

private const val DATA_PREFERENCES_NAME = "game_data"
private const val REQUESTED_EXTERNAL_KEY = "requested_external"
private const val ACTIVE_EXTERNAL_KEY = "active_external"
private const val EXTERNAL_DIRECTORY_NAME = "Pokemon Emerald"

object GameDataSettings {
    private val gameDataEntries = arrayOf("pokeemerald.sav", "storage", "profiles")

    @JvmStatic
    fun prepare(context: Context): File {
        val preferences = context.getSharedPreferences(DATA_PREFERENCES_NAME, Context.MODE_PRIVATE)
        val activeExternal = preferences.getBoolean(ACTIVE_EXTERNAL_KEY, false)
        val requestedExternal = preferences.getBoolean(REQUESTED_EXTERNAL_KEY, activeExternal)
        val activeDirectory = activeDirectory(context, activeExternal)
        val requestedDirectory = directory(context, requestedExternal)

        if (requestedDirectory == null || requestedExternal == activeExternal)
            return activeDirectory
        return try {
            mirrorGameData(activeDirectory, requestedDirectory)
            preferences.edit().putBoolean(ACTIVE_EXTERNAL_KEY, requestedExternal).commit()
            requestedDirectory
        } catch (_: IOException) {
            activeDirectory
        }
    }

    fun requestedExternal(context: Context): Boolean =
        context.getSharedPreferences(DATA_PREFERENCES_NAME, Context.MODE_PRIVATE)
            .getBoolean(REQUESTED_EXTERNAL_KEY, false)

    fun activeExternal(context: Context): Boolean =
        context.getSharedPreferences(DATA_PREFERENCES_NAME, Context.MODE_PRIVATE)
            .getBoolean(ACTIVE_EXTERNAL_KEY, false)

    fun requestExternal(context: Context, external: Boolean) {
        context.getSharedPreferences(DATA_PREFERENCES_NAME, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(REQUESTED_EXTERNAL_KEY, external)
            .apply()
    }

    fun externalDirectory(context: Context): File? = directory(context, true)

    private fun directory(context: Context, external: Boolean): File? {
        if (!external)
            return context.filesDir
        val externalRoot = context.getExternalFilesDir(null) ?: return null
        return File(externalRoot, EXTERNAL_DIRECTORY_NAME)
    }

    private fun activeDirectory(context: Context, external: Boolean): File {
        if (!external)
            return context.filesDir
        val current = directory(context, true) ?: return context.filesDir
        val legacy = legacyExternalDirectory(context)

        if (legacy != null && !containsGameData(current) && containsGameData(legacy)) {
            return try {
                mirrorGameData(legacy, current)
                current
            } catch (_: IOException) {
                legacy
            }
        }
        return if (current.isDirectory || current.mkdirs()) current else context.filesDir
    }

    private fun legacyExternalDirectory(context: Context): File? {
        val externalFiles = context.getExternalFilesDir(null) ?: return null
        val androidDirectory = externalFiles.parentFile?.parentFile?.parentFile ?: return null

        if (androidDirectory.name != "Android")
            return null
        return File(androidDirectory, "media/${context.packageName}/$EXTERNAL_DIRECTORY_NAME")
    }

    private fun containsGameData(root: File): Boolean =
        gameDataEntries.any { File(root, it).exists() }

    @Throws(IOException::class)
    internal fun mirrorGameData(sourceRoot: File, destinationRoot: File) {
        if (sourceRoot.canonicalFile == destinationRoot.canonicalFile)
            return
        if (!destinationRoot.isDirectory && !destinationRoot.mkdirs())
            throw IOException("Could not create ${destinationRoot.path}")

        val staged = mutableListOf<Pair<File, File>>()
        try {
            gameDataEntries.forEachIndexed { index, name ->
                val source = File(sourceRoot, name)
                val stage = File(destinationRoot, ".pokeemerald-migrate-$index")

                stage.deleteRecursively()
                if (source.exists()) {
                    if (!source.copyRecursively(stage, overwrite = true))
                        throw IOException("Could not copy ${source.path}")
                    staged += stage to File(destinationRoot, name)
                }
            }
            gameDataEntries.forEach { File(destinationRoot, it).deleteRecursively() }
            staged.forEach { (stage, destination) ->
                if (!stage.renameTo(destination))
                    throw IOException("Could not install ${destination.path}")
            }
        } finally {
            gameDataEntries.indices.forEach {
                File(destinationRoot, ".pokeemerald-migrate-$it").deleteRecursively()
            }
        }
    }
}
