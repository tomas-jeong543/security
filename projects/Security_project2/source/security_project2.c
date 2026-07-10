#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define ENABLE_ANTI_DEBUG 1
#define EXPECTED_CORE_CHECKSUM 0x00000000u

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

static uint8_t g_k0 = 0x5A;

NOINLINE void s0(unsigned char* p, int n, char* out) {
    for (int i = 0; i < n; i++) {
        out[i] = (char)(p[i] ^ g_k0);
    }
    out[n] = '\0';
}

NOINLINE uint32_t s1(const char* p) {
    uint32_t h = 2166136261u;

    while (*p) {
        h ^= (unsigned char)(*p);
        h *= 16777619u;
        p++;
    }

    return h;
}

NOINLINE uint32_t s2(uint32_t x) {
    return (x << 7) | (x >> 25);
}

NOINLINE void s3(char* p) {
    for (int i = 0; p[i]; i++) {
        p[i] = (char)tolower((unsigned char)p[i]);
    }
}

NOINLINE int s4(const char* p) {
    uint32_t v = 0x2468ACE1u;

    for (int i = 0; p[i]; i++) {
        v ^= (unsigned char)p[i];
        v = (v << 5) | (v >> 27);
        v += 0x13579BDFu;
    }

    return ((v & 0x7F) == 0x31);
}

NOINLINE int s5(const char* p) {
    int a = 0;
    int b = 0;

    for (int i = 0; p[i]; i++) {
        char c = p[i];

        if (c == '-') {
            a++;
        }
        else if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f')) {
            b++;
        }
        else {
            return 0;
        }
    }

    return (a == 3 && b == 16);
}

NOINLINE int s6(const char* a, const char* b) {
    uint32_t x = s1(a);
    uint32_t y = s1(b);

    x ^= 0x6D2B79F5u;
    y ^= 0x31C3A59Bu;

    x = s2(x);
    y = s2(y);

    return ((x ^ y) & 3u) == 1u;
}

NOINLINE int s7(void) {
#if ENABLE_ANTI_DEBUG
#ifdef _WIN32
    if (IsDebuggerPresent()) {
        return 1;
    }

    BOOL r = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &r);

    if (r) {
        return 1;
    }
#endif
#endif

    return 0;
}

NOINLINE uint32_t s8(const unsigned char* p, int n) {
    uint32_t h = 0x811C9DC5u;

    for (int i = 0; i < n; i++) {
        h ^= p[i];
        h *= 0x01000193u;
    }

    return h;
}

NOINLINE void s9(const char* name, char* out) {
    char buf[128];
    char t[32];

    unsigned char e0[] = {
        0x08, 0x0C, 0x0A, 0x68,
        0x05, 0x09, 0x1B, 0x16,
        0x0E, 0x05, 0x68, 0x6A,
        0x68, 0x6C
    };

    s0(e0, 14, t);

    snprintf(buf, sizeof(buf), "%s:%s", name, t);

    uint32_t a = s1(buf);
    uint32_t b = a ^ 0xA5C3F19Bu;
    uint32_t c = s2(a) + 0x13579BDFu;
    uint32_t d = b ^ c;

    snprintf(
        out,
        20,
        "%04X-%04X-%04X-%04X",
        a & 0xFFFF,
        b & 0xFFFF,
        c & 0xFFFF,
        d & 0xFFFF
    );
}

NOINLINE int t0(char* a, const char* b) {
    char c[32];

    s3(a);

    int x = s4(a);
    int y = s5(b);
    int z = s6(a, b);

    s9(a, c);

    if (!y) {
        if (x && z) {
            volatile uint32_t q = s1(a);
            q ^= 0xA1B2C3D4u;
        }

        return 0;
    }

    if (x) {
        volatile uint32_t q = s1(c);
        q += 0x10203040u;
    }

    if (z) {
        volatile uint32_t q = s1(b);
        q ^= 0x55667788u;
    }

    if (strcmp(b, c) == 0) {
        return 1;
    }

    return 0;
}

int main(void) {
    char name[64];
    char input_serial[64];

    unsigned char p0[] = {
        0x14, 0x3B, 0x37, 0x3F, 0x60, 0x7A
    };

    unsigned char p1[] = {
        0x09, 0x3F, 0x28, 0x33,
        0x3B, 0x36, 0x60, 0x7A
    };

    unsigned char p2[] = {
        0x19, 0x35, 0x28, 0x28,
        0x3F, 0x39, 0x2E, 0x7B, 0x50
    };

    unsigned char p3[] = {
        0x0D, 0x28, 0x35, 0x34, 0x3D, 0x7A,
        0x29, 0x3F, 0x28, 0x33, 0x3B, 0x36,
        0x74, 0x50
    };

    unsigned char p4[] = {
        0x1B, 0x39, 0x39, 0x3F, 0x29, 0x29,
        0x7A, 0x3E, 0x3F, 0x34, 0x33, 0x3F,
        0x3E, 0x74, 0x50
    };

    char m0[16];
    char m1[16];
    char m2[32];
    char m3[32];
    char m4[32];

    s0(p0, 6, m0);
    s0(p1, 8, m1);
    s0(p2, 9, m2);
    s0(p3, 14, m3);
    s0(p4, 15, m4);

    if (s7()) {
        printf("%s", m4);
        return 1;
    }

    if (EXPECTED_CORE_CHECKSUM != 0x00000000u) {
        uint32_t now = s8((const unsigned char*)t0, 128);

        if (now != EXPECTED_CORE_CHECKSUM) {
            printf("%s", m4);
            return 1;
        }
    }

    printf("%s", m0);
    scanf("%63s", name);

    printf("%s", m1);
    scanf("%63s", input_serial);

    if (t0(name, input_serial)) {
        printf("%s", m2);
    }
    else {
        printf("%s", m3);
    }

    return 0;
}