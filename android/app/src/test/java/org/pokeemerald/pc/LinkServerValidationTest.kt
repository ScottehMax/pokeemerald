package org.pokeemerald.pc

import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class LinkServerValidationTest {
    @Test
    fun acceptsHostNamesAndIpv4Addresses() {
        assertNull(validateLinkServer("link.example.com"))
        assertNull(validateLinkServer("link.example.com:8765"))
        assertNull(validateLinkServer("192.0.2.10:1"))
        assertNull(validateLinkServer("192.0.2.10:65535"))
    }

    @Test
    fun rejectsUrlsAndWhitespace() {
        assertNotNull(validateLinkServer(""))
        assertNotNull(validateLinkServer("https://link.example.com"))
        assertNotNull(validateLinkServer("link.example.com/path"))
        assertNotNull(validateLinkServer("link example.com"))
    }

    @Test
    fun rejectsInvalidPortsAndIpv6() {
        assertNotNull(validateLinkServer("link.example.com:"))
        assertNotNull(validateLinkServer("link.example.com:0"))
        assertNotNull(validateLinkServer("link.example.com:65536"))
        assertNotNull(validateLinkServer("[2001:db8::1]:8765"))
    }
}
