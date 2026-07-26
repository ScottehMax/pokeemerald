package org.pokeemerald.pc

import java.io.File
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class GameDataSettingsTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun replacesSaveProfilesAndStorageOverStaleDestinationData() {
        val source = temporaryFolder.newFolder("source")
        val destination = temporaryFolder.newFolder("destination")
        val save = byteArrayOf(1, 2, 3)
        val defaultMon = byteArrayOf(4, 5, 6)
        val profileSave = byteArrayOf(7, 8, 9)
        val profileMon = byteArrayOf(10, 11, 12)

        write(source, "pokeemerald.sav", save)
        write(source, "storage/default.ek3", defaultMon)
        write(source, "profiles/A/pokeemerald.sav", profileSave)
        write(source, "profiles/A/storage/profile.ek3", profileMon)
        write(destination, "pokeemerald.sav", byteArrayOf(99))
        write(destination, "profiles/stale/pokeemerald.sav", byteArrayOf(99))

        GameDataSettings.replaceGameData(source, destination)

        assertArrayEquals(save, File(destination, "pokeemerald.sav").readBytes())
        assertArrayEquals(defaultMon, File(destination, "storage/default.ek3").readBytes())
        assertArrayEquals(profileSave, File(destination, "profiles/A/pokeemerald.sav").readBytes())
        assertArrayEquals(profileMon, File(destination, "profiles/A/storage/profile.ek3").readBytes())
        assertFalse(File(destination, "profiles/stale").exists())
    }

    @Test
    fun removesGameDataButLeavesUnrelatedFiles() {
        val source = temporaryFolder.newFolder("remove-source")

        write(source, "pokeemerald.sav", byteArrayOf(1))
        write(source, "storage/default.ek3", byteArrayOf(2))
        write(source, "profiles/A/pokeemerald.sav", byteArrayOf(3))
        write(source, ".last-profile", byteArrayOf(4))
        write(source, "crash-reports/report.txt", byteArrayOf(5))

        GameDataSettings.removeGameData(source)

        assertFalse(File(source, "pokeemerald.sav").exists())
        assertFalse(File(source, "storage").exists())
        assertFalse(File(source, "profiles").exists())
        assertFalse(File(source, ".last-profile").exists())
        assertArrayEquals(byteArrayOf(5), File(source, "crash-reports/report.txt").readBytes())
    }

    private fun write(root: File, path: String, data: ByteArray) {
        File(root, path).apply {
            parentFile?.mkdirs()
            writeBytes(data)
        }
    }
}
