package org.pokeemerald.pc

import android.content.Context
import java.io.File
import java.io.IOException

private const val DATA_PREFERENCES_NAME = "game_data"
private const val REQUESTED_EXTERNAL_KEY = "requested_external"
private const val ACTIVE_EXTERNAL_KEY = "active_external"
private const val EXTERNAL_DIRECTORY_NAME = "Pokemon Emerald"

object GameDataSettings {
    private val gameDataEntries =
        arrayOf("pokeemerald.sav", "storage", "profiles", ".last-profile")

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
            replaceGameData(activeDirectory, requestedDirectory)
            if (!preferences.edit().putBoolean(ACTIVE_EXTERNAL_KEY, requestedExternal).commit())
                throw IOException("Could not save the active game-data location")
            removeGameData(activeDirectory)
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
        val current = directory(context, external) ?: return context.filesDir

        return if (!external || current.isDirectory || current.mkdirs()) current else context.filesDir
    }

    @Throws(IOException::class)
    internal fun replaceGameData(sourceRoot: File, destinationRoot: File) {
        if (sourceRoot.canonicalFile == destinationRoot.canonicalFile)
            return
        if (!destinationRoot.isDirectory && !destinationRoot.mkdirs())
            throw IOException("Could not create ${destinationRoot.path}")

        val staged = mutableListOf<Pair<File, File>>()
        try {
            gameDataEntries.forEachIndexed { index, name ->
                val source = File(sourceRoot, name)
                val stage = File(destinationRoot, ".pokeemerald-stage-$index")

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
                File(destinationRoot, ".pokeemerald-stage-$it").deleteRecursively()
            }
        }
    }

    internal fun removeGameData(sourceRoot: File) {
        gameDataEntries.forEach { File(sourceRoot, it).deleteRecursively() }
        sourceRoot.delete()
    }
}
