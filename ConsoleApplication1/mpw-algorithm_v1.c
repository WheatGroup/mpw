//
//  mpw-algorithm.c
//  MasterPassword
//
//  Created by Maarten Billemont on 2014-12-20.
//  Copyright (c) 2014 Lyndir. All rights reserved.
//

#include <string.h>
#include <errno.h>
#ifdef GIN_BUILD
#ifdef _WIN32
#include <winsock2.h>
#endif
#else
#include <arpa/inet.h>
#endif

#include "mpw-types.h"
#include "mpw-util.h"

#define MP_N                32768
#define MP_r                8
#define MP_p                2
#define MP_hash             PearlHashSHA256

static const uint8_t *mpw_masterKeyForUser_v1(const char *fullName, const char *masterPassword) {

    const char *mpKeyScope = mpw_scopeForVariant( MPSiteVariantPassword );
    trc( "algorithm: v%d\n", 1 );
    trc( "fullName: %s (%zu)\n", fullName, mpw_utf8_strlen( fullName ) );
    trc( "masterPassword: %s\n", masterPassword );
    trc( "key scope: %s\n", mpKeyScope );

    // Calculate the master key salt.
    // masterKeySalt = mpKeyScope . #fullName . fullName
    size_t masterKeySaltSize = 0;
    uint8_t *masterKeySalt = NULL;
    mpw_push_string( &masterKeySalt, &masterKeySaltSize, mpKeyScope );
    mpw_push_int( &masterKeySalt, &masterKeySaltSize, htonl( mpw_utf8_strlen( fullName ) ) );
    mpw_push_string( &masterKeySalt, &masterKeySaltSize, fullName );
    if (!masterKeySalt) {
        ftl( "Could not allocate master key salt: %d\n", errno );
        return NULL;
    }
    trc( "masterKeySalt ID: %s\n", mpw_id_buf( masterKeySalt, masterKeySaltSize ) );

    // Calculate the master key.
    // masterKey = scrypt( masterPassword, masterKeySalt )
    const uint8_t *masterKey = mpw_scrypt( MP_dkLen, masterPassword, masterKeySalt, masterKeySaltSize, MP_N, MP_r, MP_p );
    mpw_free( masterKeySalt, masterKeySaltSize );
    if (!masterKey) {
        ftl( "Could not allocate master key: %d\n", errno );
        return NULL;
    }
    trc( "masterKey ID: %s\n", mpw_id_buf( masterKey, MP_dkLen ) );

    return masterKey;
}

static const char *mpw_passwordForSite_v1(const uint8_t *masterKey, const char *siteName, const MPSiteType siteType, const uint32_t siteCounter,
        const MPSiteVariant siteVariant, const char *siteContext) {

    const char *siteScope = mpw_scopeForVariant( siteVariant );
    trc( "algorithm: v%d\n", 1 );
    trc( "siteName: %s\n", siteName );
    trc( "siteCounter: %d\n", siteCounter );
    trc( "siteVariant: %d\n", siteVariant );
    trc( "siteType: %d\n", siteType );
    trc( "site scope: %s, context: %s\n", siteScope, siteContext? "<empty>": siteContext );
    trc( "seed from: hmac-sha256(masterKey, %s | %s | %s | %s | %s | %s)\n",
            siteScope, mpw_hex_l( htonl( strlen( siteName ) ) ), siteName,
            mpw_hex_l( htonl( siteCounter ) ),
            mpw_hex_l( htonl( siteContext? strlen( siteContext ): 0 ) ), siteContext? "(null)": siteContext );

    // Calculate the site seed.
    // sitePasswordSeed = hmac-sha256( masterKey, siteScope . #siteName . siteName . siteCounter . #siteContext . siteContext )
    size_t sitePasswordInfoSize = 0;
    uint8_t *sitePasswordInfo = NULL;
    mpw_push_string( &sitePasswordInfo, &sitePasswordInfoSize, siteScope );
    mpw_push_int( &sitePasswordInfo, &sitePasswordInfoSize, htonl( mpw_utf8_strlen( siteName ) ) );
    mpw_push_string( &sitePasswordInfo, &sitePasswordInfoSize, siteName );
    mpw_push_int( &sitePasswordInfo, &sitePasswordInfoSize, htonl( siteCounter ) );
    if (siteContext) {
        mpw_push_int( &sitePasswordInfo, &sitePasswordInfoSize, htonl( mpw_utf8_strlen( siteContext ) ) );
        mpw_push_string( &sitePasswordInfo, &sitePasswordInfoSize, siteContext );
    }
    if (!sitePasswordInfo) {
        ftl( "Could not allocate site seed info: %d\n", errno );
        return NULL;
    }
    trc( "sitePasswordInfo ID: %s\n", mpw_id_buf( sitePasswordInfo, sitePasswordInfoSize ) );

    const uint8_t *sitePasswordSeed = mpw_hmac_sha256( masterKey, MP_dkLen, sitePasswordInfo, sitePasswordInfoSize );
    mpw_free( sitePasswordInfo, sitePasswordInfoSize );
    if (!sitePasswordSeed) {
        ftl( "Could not allocate site seed: %d\n", errno );
        return NULL;
    }
    trc( "sitePasswordSeed ID: %s\n", mpw_id_buf( sitePasswordSeed, 32 ) );

    // Determine the template.
    const char *template1 = mpw_templateForType( siteType, sitePasswordSeed[0] );
    trc( "type %d, template1: %s\n", siteType, template1 );
    if (strlen( template1 ) > 32) {
        ftl( "Template too long for password seed: %lu", strlen( template1 ) );
        mpw_free( sitePasswordSeed, sizeof( sitePasswordSeed ) );
        return NULL;
    }

    // Encode the password from the seed using the template.
    char *const sitePassword = (char *const)calloc( strlen( template1 ) + 1, sizeof( char ) );
    for (size_t c = 0; c < strlen( template1 ); ++c) {
        sitePassword[c] = mpw_characterFromClass( template1[c], sitePasswordSeed[c + 1] );
        trc( "class %c, index %u (0x%02X) -> character: %c\n", template1[c], sitePasswordSeed[c + 1], sitePasswordSeed[c + 1],
                sitePassword[c] );
    }
    mpw_free( sitePasswordSeed, sizeof( sitePasswordSeed ) );

    return sitePassword;
}
