/* crypto/rand/rand_win.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include "internal/cryptlib.h"
#include <openssl/rand.h>
#include "rand_lcl.h"

#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_WIN32)
# include <windows.h>
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0400
# endif
# include <wincrypt.h>
# include <tlhelp32.h>

/*
 * Limit the time spent walking through the heap, processes, threads and
 * modules to a maximum of 1000 miliseconds each, unless CryptoGenRandom
 * failed
 */
# define MAXDELAY 1000

/*
 * Intel hardware RNG CSP -- available from
 * http://developer.intel.com/design/security/rng/redist_license.htm
 */
# define PROV_INTEL_SEC 22
# define INTEL_DEF_PROV L"Intel Hardware Cryptographic Service Provider"

static void readtimer(void);
static void readscreen(void);

/*
 * It appears like CURSORINFO, PCURSORINFO and LPCURSORINFO are only defined
 * when WINVER is 0x0500 and up, which currently only happens on Win2000.
 * Unfortunately, those are typedefs, so they're a little bit difficult to
 * detect properly.  On the other hand, the macro CURSOR_SHOWING is defined
 * within the same conditional, so it can be use to detect the absence of
 * said typedefs.
 */

# ifndef CURSOR_SHOWING
/*
 * Information about the global cursor.
 */
typedef struct tagCURSORINFO {
    DWORD cbSize;
    DWORD flags;
    HCURSOR hCursor;
    POINT ptScreenPos;
} CURSORINFO, *PCURSORINFO, *LPCURSORINFO;

#  define CURSOR_SHOWING     0x00000001
# endif                         /* CURSOR_SHOWING */

# if !defined(OPENSSL_SYS_WINCE)
typedef BOOL(WINAPI *CRYPTACQUIRECONTEXTW) (HCRYPTPROV *, LPCWSTR, LPCWSTR,
                                            DWORD, DWORD);
typedef BOOL(WINAPI *CRYPTGENRANDOM) (HCRYPTPROV, DWORD, BYTE *);
typedef BOOL(WINAPI *CRYPTRELEASECONTEXT) (HCRYPTPROV, DWORD);

typedef HWND(WINAPI *GETFOREGROUNDWINDOW) (VOID);
typedef BOOL(WINAPI *GETCURSORINFO) (PCURSORINFO);
typedef DWORD(WINAPI *GETQUEUESTATUS) (UINT);

typedef HANDLE(WINAPI *CREATETOOLHELP32SNAPSHOT) (DWORD, DWORD);
typedef BOOL(WINAPI *CLOSETOOLHELP32SNAPSHOT) (HANDLE);
typedef BOOL(WINAPI *HEAP32FIRST) (LPHEAPENTRY32, DWORD, size_t);
typedef BOOL(WINAPI *HEAP32NEXT) (LPHEAPENTRY32);
typedef BOOL(WINAPI *HEAP32LIST) (HANDLE, LPHEAPLIST32);
typedef BOOL(WINAPI *PROCESS32) (HANDLE, LPPROCESSENTRY32);
typedef BOOL(WINAPI *THREAD32) (HANDLE, LPTHREADENTRY32);
typedef BOOL(WINAPI *MODULE32) (HANDLE, LPMODULEENTRY32);

#  include <lmcons.h>
#  include <lmstats.h>
/*
 * The NET API is Unicode only.  It requires the use of the UNICODE macro.
 * When UNICODE is defined LPTSTR becomes LPWSTR.  LMSTR was was added to the
 * Platform SDK to allow the NET API to be used in non-Unicode applications
 * provided that Unicode strings were still used for input.  LMSTR is defined
 * as LPWSTR.
 */
typedef NET_API_STATUS(NET_API_FUNCTION *NETSTATGET)
 (LPWSTR, LPWSTR, DWORD, DWORD, LPBYTE *);
typedef NET_API_STATUS(NET_API_FUNCTION *NETFREE) (LPBYTE);
# endif                         /* !OPENSSL_SYS_WINCE */

int RAND_poll(void)
{
    HCRYPTPROV hProvider = 0;
    DWORD w, sgx_rand_val;
    int good = 0, i;

# if defined(OPENSSL_SYS_WINCE)
#  if defined(_WIN32_WCE) && _WIN32_WCE>=300
    /*
     * Even though MSDN says _WIN32_WCE>=210, it doesn't seem to be available
     * in commonly available implementations prior 300...
     */
    {
        BYTE buf[64];
        /* poll the CryptoAPI PRNG */
        /* The CryptoAPI returns sizeof(buf) bytes of randomness */
        if (CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL,
                                 CRYPT_VERIFYCONTEXT)) {
            if (CryptGenRandom(hProvider, sizeof(buf), buf))
                RAND_add(buf, sizeof(buf), sizeof(buf));
            CryptReleaseContext(hProvider, 0);
        }
    }
#  endif
# else                          /* OPENSSL_SYS_WINCE */
    /*
     * None of below libraries are present on Windows CE, which is
     * why we #ifndef the whole section. This also excuses us from
     * handling the GetProcAddress issue. The trouble is that in
     * real Win32 API GetProcAddress is available in ANSI flavor
     * only. In WinCE on the other hand GetProcAddress is a macro
     * most commonly defined as GetProcAddressW, which accepts
     * Unicode argument. If we were to call GetProcAddress under
     * WinCE, I'd recommend to either redefine GetProcAddress as
     * GetProcAddressA (there seem to be one in common CE spec) or
     * implement own shim routine, which would accept ANSI argument
     * and expand it to Unicode.
     */
    {
        BYTE buf[64];

        /* The CryptoAPI returns sizeof(buf) bytes of randomness */
        if (sgx_CryptAcquireContext(&hProvider, NULL, NULL, PROV_RSA_FULL,
                    CRYPT_VERIFYCONTEXT)) {
            if (sgx_CryptGenRandom(hProvider, sizeof(buf), buf) != 0) {
                RAND_add(buf, sizeof(buf), 0);
                good = 1;
            }
            sgx_CryptReleaseContext(hProvider, 0);
        }

        /* poll the Pentium PRG with CryptoAPI */
        if (sgx_CryptAcquireContext(&hProvider, 0, INTEL_DEF_PROV, PROV_INTEL_SEC, 0)) {
            if (sgx_CryptGenRandom(hProvider, sizeof(buf), buf) != 0) {
                RAND_add(buf, sizeof(buf), sizeof(buf));
                good = 1;
            }
            sgx_CryptReleaseContext(hProvider, 0);
        }
    }
# endif                         /* !OPENSSL_SYS_WINCE */

    /* timer data */
    readtimer();

    // SGX: increase entropy using sgx_read_rand function
    for(i=0;i<50;i++) {
        sgx_read_rand((unsigned char *)&sgx_rand_val, sizeof(DWORD));
        RAND_add(&sgx_rand_val, sizeof(DWORD), 1);
    }

    /* process ID */
    w = sgx_getpid();
    RAND_add(&w, sizeof(w), 1);

    return (1);
}
#if 0 // TOR_SGX
int RAND_event(UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    double add_entropy = 0;

    switch (iMsg) {
    case WM_KEYDOWN:
        {
            static WPARAM key;
            if (key != wParam)
                add_entropy = 0.05;
            key = wParam;
        }
        break;
    case WM_MOUSEMOVE:
        {
            static int lastx, lasty, lastdx, lastdy;
            int x, y, dx, dy;

            x = LOWORD(lParam);
            y = HIWORD(lParam);
            dx = lastx - x;
            dy = lasty - y;
            if (dx != 0 && dy != 0 && dx - lastdx != 0 && dy - lastdy != 0)
                add_entropy = .2;
            lastx = x, lasty = y;
            lastdx = dx, lastdy = dy;
        }
        break;
    }

    readtimer();
    RAND_add(&iMsg, sizeof(iMsg), add_entropy);
    RAND_add(&wParam, sizeof(wParam), 0);
    RAND_add(&lParam, sizeof(lParam), 0);

    return (RAND_status());
}
#endif //TOR_SGX
void RAND_screen(void)
{                               /* function available for backward
                                 * compatibility */
    RAND_poll();
    readscreen();
}

/* feed timing information to the PRNG */
static void readtimer(void)
{
#if 0 // TOR_SGX
    DWORD w;
    LARGE_INTEGER l;
    static int have_perfc = 1;
# if defined(_MSC_VER) && defined(_M_X86)
    static int have_tsc = 1;
    DWORD cyclecount;

    if (have_tsc) {
        __try {
            __asm {
            _emit 0x0f _emit 0x31 mov cyclecount, eax}
            RAND_add(&cyclecount, sizeof(cyclecount), 1);
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            have_tsc = 0;
        }
    }
# else
#  define have_tsc 0
# endif

    if (have_perfc) {
        if (QueryPerformanceCounter(&l) == 0)
            have_perfc = 0;
        else
            RAND_add(&l, sizeof(l), 0);
    }

    if (!have_tsc && !have_perfc) {
        w = GetTickCount();
        RAND_add(&w, sizeof(w), 0);
    }
#endif
}

/* feed screen contents to PRNG */
/*****************************************************************************
 *
 * Created 960901 by Gertjan van Oosten, gertjan@West.NL, West Consulting B.V.
 *
 * Code adapted from
 * <URL:http://support.microsoft.com/default.aspx?scid=kb;[LN];97193>;
 * the original copyright message is:
 *
 *   (C) Copyright Microsoft Corp. 1993.  All rights reserved.
 *
 *   You have a royalty-free right to use, modify, reproduce and
 *   distribute the Sample Files (and/or any modified version) in
 *   any way you find useful, provided that you agree that
 *   Microsoft has no warranty obligations or liability for any
 *   Sample Application Files which are modified.
 */

static void readscreen(void)
{
    #if 0 // TOR_SGX
# if !defined(OPENSSL_SYS_WINCE) && !defined(OPENSSL_SYS_WIN32_CYGWIN)
    HDC hScrDC;                 /* screen DC */
    HBITMAP hBitmap;            /* handle for our bitmap */
    BITMAP bm;                  /* bitmap properties */
    unsigned int size;          /* size of bitmap */
    char *bmbits;               /* contents of bitmap */
    int w;                      /* screen width */
    int h;                      /* screen height */
    int y;                      /* y-coordinate of screen lines to grab */
    int n = 16;                 /* number of screen lines to grab at a time */
    BITMAPINFOHEADER bi;        /* info about the bitmap */

    if (check_winnt() && OPENSSL_isservice() > 0)
        return;

    /* Get a reference to the screen DC */
    hScrDC = GetDC(NULL);

    /* Get screen resolution */
    w = GetDeviceCaps(hScrDC, HORZRES);
    h = GetDeviceCaps(hScrDC, VERTRES);

    /* Create a bitmap compatible with the screen DC */
    hBitmap = CreateCompatibleBitmap(hScrDC, w, n);

    /* Get bitmap properties */
    GetObject(hBitmap, sizeof(BITMAP), (LPSTR) & bm);
    size = (unsigned int)bm.bmWidthBytes * bm.bmHeight * bm.bmPlanes;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = bm.bmPlanes;
    bi.biBitCount = bm.bmBitsPixel;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    bmbits = OPENSSL_malloc(size);
    if (bmbits) {
        /* Now go through the whole screen, repeatedly grabbing n lines */
        for (y = 0; y < h - n; y += n) {
            unsigned char md[MD_DIGEST_LENGTH];

            /* Copy the bits of the current line range into the buffer */
            GetDIBits(hScrDC, hBitmap, y, n,
                      bmbits, (BITMAPINFO *) & bi, DIB_RGB_COLORS);

            /* Get the hash of the bitmap */
            MD(bmbits, size, md);

            /* Seed the random generator with the hash value */
            RAND_add(md, MD_DIGEST_LENGTH, 0);
        }

        OPENSSL_free(bmbits);
    }

    /* Clean up */
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hScrDC);
# endif                         /* !OPENSSL_SYS_WINCE */
#endif //TOR_SGX
}

#endif
