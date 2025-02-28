//
//  main.c
//  tsschecker
//
//  Created by tihmstar on 22.12.15.
//  Copyright © 2015 tihmstar. All rights reserved.
//

#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include <libfragmentzip/libfragmentzip.h>
#include "download.h"
#include "debug.h"
#include "tsschecker.h"
#include "all.h"

#define FLAG_LIST_IOS       (1 << 0)
#define FLAG_LIST_DEVICES   (1 << 1)
#define FLAG_BUILDMANIFEST  (1 << 2)
#define FLAG_LATEST_IOS     (1 << 3)

int idevicerestore_debug;
#define reterror(code,a ...) {error(a); err = code; goto error;}

static struct option longopts[] = {
    { "build-manifest",     required_argument, NULL, 'm' },
    { "device",             required_argument, NULL, 'd' },
    { "ios",                required_argument, NULL, 'i' },
    { "ecid",               required_argument, NULL, 'e' },
    { "help",               no_argument,       NULL, 'h' },
    { "no-baseband",        optional_argument, NULL, 'b' },
    { "ota",                no_argument,       NULL, 'o' },
    { "save",               no_argument,       NULL, 's' },
    { "latest",             no_argument,       NULL, 'l' },
    { "update-install",     optional_argument, NULL, 'u' },
    { "boardconfig",        required_argument, NULL, 'B' },
    { "buildid",            required_argument, NULL, 'Z' },
    { "debug",              no_argument,       NULL,  0  },
    { "list-devices",       no_argument,       NULL,  1  },
    { "list-ios",           no_argument,       NULL,  2  },
    { "save-path",          required_argument, NULL,  3  },
    { "print-tss-request",  no_argument,       NULL,  4  },
    { "print-tss-response", no_argument,       NULL,  5  },
    { "beta",               no_argument,       NULL,  6  },
    { "nocache",            no_argument,       NULL,  7  },
    { "apnonce",            required_argument, NULL,  8  },
    { "sepnonce",           required_argument, NULL,  9  },
    { "raw",                required_argument, NULL, 10  },
    { "bbsnum",             required_argument, NULL, 11  },
    { "generator",          required_argument, NULL, 'g' },
    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf("Usage: tsschecker [OPTIONS]\n");
    printf("Checks (real) signing status of device/firmware\n\n");
    printf("  -d, --device MODEL\t\tspecific device by its model (eg. iPhone4,1)\n");
    printf("  -i, --ios VERSION\t\tspecific firmware version (eg. 6.1.3)\n");
    printf("  -Z  --buildid\t\t\tspecific buildid instead of firmware version (eg. 13C75)\n");
    printf("  -B, --boardconfig\t\tspecific boardconfig instead of device model (eg. n61ap)\n");
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("  -o, --ota\t\t\tcheck OTA signing status, instead of normal restore\n");
    printf("  -b, --no-baseband\t\tdon't check baseband signing status. Request a ticket without baseband\n");
    printf("  -m, --build-manifest\t\tmanually specify buildmanifest (can be used with -d)\n");
    printf("  -s, --save\t\t\tsave fetched shsh blobs (mostly makes sense with -e)\n");
    printf("  -u, --update-install\t\trequest update ticket instead of erase\n");
    printf("  -l, --latest\t\t\tuse latest public firmware version instead of manually specifying one\n");
    printf("                 \t\tespecially useful with -s and -e for saving blobs\n");
    printf("  -e, --ecid ECID\t\tmanually specify ECID to be used for fetching blobs, instead of using random ones\n");
    printf("                 \t\tECID must be either DEC or HEX eg. 5482657301265 or ab46efcbf71\n");
    printf("      --apnonce NONCE\t\tmanually specify ApNonce instead of using random one (not required for saving blobs)\n");
    printf("      --sepnonce NONCE\t\tmanually specify SepNonce instead of using random one (not required for saving blobs)\n");
    printf("      --bbsnum SNUM\t\tmanually specify BbSNUM in HEX for saving valid BBTicket (not required for saving blobs)\n");
    printf("  -g, --generator GEN\t\tmanually specify generator in format 0x%%16llx\n");
    printf("      --save-path PATH\t\tspecify path for saving blobs\n");
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("      --beta\t\t\trequest ticket for beta instead of normal release (use with -o)\n");
    printf("      --list-devices\t\tlist all known devices\n");
    printf("      --list-ios\t\tlist all known firmware versions\n");
    printf("      --nocache \t\tignore caches and redownload required files\n");
    printf("      --print-tss-request\tprint TSS request that will be sent to Apple\n");
    printf("      --print-tss-response\tprint TSS response that come from Apple\n");
    printf("      --raw\t\t\tsend raw file to Apple's TSS server (useful for debugging)\n\n");
}

int64_t parseECID(const char *ecid){
    const char *ecidBK = ecid;
    int isHex = 0;
    int64_t ret = 0;
    
    //in case hex ecid only contains digits, specify with 0x1235
    if (strncmp(ecid, "0x", 2) == 0){
        isHex = 1;
        ecidBK = ecid+2;
    }
    
    while (*ecid && !isHex) {
        char c = *(ecid++);
        if (c >= '0' && c<='9') {
            ret *=10;
            ret += c - '0';
        }else{
            isHex = 1;
            ret = 0;
        }
    }
    
    if (isHex) {
        while (*ecidBK) {
            char c = *(ecidBK++);
            ret *=16;
            if (c >= '0' && c<='9') {
                ret += c - '0';
            }else if (c >= 'a' && c <= 'f'){
                ret += 10 + c - 'a';
            }else if (c >= 'A' && c <= 'F'){
                ret += 10 + c - 'A';
            }else{
                return 0; //ERROR parsing failed
            }
        }
    }
    
    return ret;
}

char *parseNonce(const char *nonce, size_t *parsedLen){
    char *ret = NULL;
    size_t retSize = 0;
    if (parseHex(nonce, parsedLen, NULL, &retSize))
        return NULL;
    ret = malloc(retSize);
    if (parseHex(nonce, parsedLen, ret, &retSize))
        return NULL;
    
    return ret;
}

int main(int argc, const char * argv[]) {
    int err = 0;
    int isSigned = 0;
    printf("Version: "TSSCHECKER_VERSION_SHA" - "TSSCHECKER_VERSION_COUNT"\n");
    printf("%s\n",fragmentzip_version());
    
    dbglog = 1;
    idevicerestore_debug = 0;
    save_shshblobs = 0;
    int optindex = 0;
    int opt = 0;
    long flags = 0;
    
    char *buildmanifest = 0;
    char *ecid = 0;
    
    char *apnonce = 0;
    char *sepnonce = 0;
    char *bbsnum = 0;
    t_devicevals devVals = {0};
    t_iosVersion versVals = {0};
    char *firmwareJson = NULL;
    jssytok_t *firmwareTokens = NULL;
    
    const char *rawFilePath = NULL;
    
    if (argc == 1){
        cmd_help();
        return -1;
    }

    while ((opt = getopt_long(argc, (char* const *)argv, "d:i:e:m:B:hg:slbuo", longopts, &optindex)) > 0) {
        switch (opt) {
            case 'h': // long option: "help"; can be called as short option
                cmd_help();
                return 0;
            case 'd': // long option: "device"; can be called as short option
                devVals.deviceModel = strdup(optarg);
                break;
            case 'i': // long option: "ios"; can be called as short option
                if (versVals.version) reterror(-9, "[TSSC] parsing parameter failed!\n");
                versVals.version = strdup(optarg);
                break;
            case 'Z': // long option: "buildid"; can be called as short option
                versVals.buildID = strdup(optarg);
                break;
            case 'B': // long option: "boardconfig"; can be called as short option
                devVals.deviceBoard = strdup(optarg);
                break;
            case 'e': // long option: "ecid"; can be called as short option
                ecid = optarg;
                break;
            case 'g': // long option: "generator"; can be called as short option
                if (optarg[0] != '0' && optarg[1] != 'x')
                    goto failparse;
                devVals.generator[0] = '0';
                devVals.generator[1] = 'x';
                devVals.generator[sizeof(devVals.generator)-1] = '\0';
                devVals.generator[0+2] = tolower(optarg[0+2]);
                for (int i=0;i<16; i++){
                    devVals.generator[i+2] = tolower(optarg[i+2]);
                    if (!isdigit(optarg[i+2]) && ((int)devVals.generator[i+2] < 'a' || devVals.generator[i+2] > 'f'))
                        failparse:
                        reterror(-10, "[TSSC] parsing generator \"%s\" failed\n",optarg);

                }
                info("[TSSC] manually specified generator \"%s\"\n",devVals.generator);
                break;
            case 'b': // long option: "no-baseband"; can be called as short option
                if (optarg) versVals.basebandMode = atoi(optarg);
                else versVals.basebandMode = kBasebandModeWithoutBaseband;
                break;
            case 'u': // long option: "update"; can be called as short option
                if (optarg) {
                    if ((devVals.installType = atoi(optarg)) > 2 || devVals.installType < 0){
                        warning("unknown installType %d. Setting installType to default (%d)\n",devVals.installType,devVals.installType = kInstallTypeDefault);
                    }
                }else
                    devVals.installType = kInstallTypeUpdate;
                if (devVals.installType)
                    printf("[TSSC] manually setting install type = %s\n",devVals.installType == kInstallTypeUpdate ? "Update" : "Erase");
                break;
            case 'l': // long option: "latest"; can be called as short option
                flags |= FLAG_LATEST_IOS;
                break;
            case 's': // long option: "save"; can be called as short option
                save_shshblobs = 1;
                break;
            case 'o': // long option: "ota"; can be called as short option
                versVals.isOta = 1;
                break;
            case 'm': // long option: "build-manifest"; can be called as short option
                flags |= FLAG_BUILDMANIFEST;
                buildmanifest = optarg;
                break;
            case 0: // only long option: "debug"
                idevicerestore_debug = 1;
                break;
            case 1: // only long option: "list-devices"
                flags |= FLAG_LIST_DEVICES;
                break;
            case 2: // only long option: "list-ios"
                flags |= FLAG_LIST_IOS;
                break;
            case 3: // only long option: "save-path"
                shshSavePath = optarg;
                break;
            case 4: // only long option: "print-tss-request"
                print_tss_request = 1;
                break;
            case 5: // only long option: "print-tss-response"
                print_tss_response = 1;
                break;
            case 6: // only long option: "beta"
                versVals.useBeta = 1;
                break;
            case 7: // only long option: "nocache"
                nocache = 1;
                break;
            case 8: // only long option: "apnonce"
                apnonce = optarg;
                break;
            case 9: // only long option: "sepnonce"
                sepnonce = optarg;
                break;
            case 10: // only long option: "raw"
                rawFilePath = optarg;
                idevicerestore_debug = 1;
                break;
            case 11: // only long option: "bbsnum"
                bbsnum = optarg;
                break;
            default:
                cmd_help();
                return -1;
        }
    }
    
    if (rawFilePath) {
        char *buf = NULL;
        size_t bufSize = 0;
        FILE *f = fopen(rawFilePath, "rb");
        if (!f)
            reterror(-100, "[TSSC] failed to read rawfile at \"%s\"\n",rawFilePath);
        fseek(f, 0, SEEK_END);
        bufSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = (char*)malloc(bufSize+1);
        fread(buf, 1, bufSize, f);
        fclose(f);
        
        printf("Sending TSS request:\n%s",buf);
        char *rsp = tss_request_send_raw(buf, NULL, (int*)&bufSize);

        printf("TSS server returned:\n%s\n",rsp);
        free(rsp);
        return 0;
    }

    if (devVals.deviceBoard)
        for (int i=0; i<strlen(devVals.deviceBoard); i++)
            devVals.deviceBoard[i] = tolower(devVals.deviceBoard[i]);
    
    if (!devVals.deviceModel){
        if (devVals.deviceBoard){
            char *tmp = NULL;
            if ((tmp = (char*)getModelFromBoardconfig(devVals.deviceBoard)))
                devVals.deviceModel = strdup(tmp);
            else
                reterror(-25, "[TSSC] If you using --boardconfig, please also specify device model with -d\n");
        }
    }
    
    if (devVals.deviceModel) {
        for (char *c = devVals.deviceModel; *c; c++)
            *c = tolower(*c); //make devicemodel lowercase
        //make devicemodel look nice. This is completely optional
        if (*devVals.deviceModel == 'i')
            devVals.deviceModel[1] = toupper(devVals.deviceModel[1]);
        else
            devVals.deviceModel[0] = toupper(devVals.deviceModel[0]);
    }
    
    if (ecid) {
        if ((devVals.ecid = parseECID(ecid)) == 0){
            reterror(-7, "[TSSC] manually specified ECID=%s, but parsing failed\n",ecid);
        }else{
            info("[TSSC] manually specified ECID to use, parsed \"%s\" to dec:%lld hex:%llx\n",ecid,devVals.ecid,devVals.ecid);
        }
    }
    
    if (apnonce) {
        if ((devVals.apnonce = parseNonce(apnonce,&devVals.parsedApnonceLen)) ){
            info("[TSSC] manually specified apnonce to use, parsed \"%s\" to hex:",apnonce);
            unsigned char *tmp = (unsigned char*)devVals.apnonce;
            for (int i=0; i< devVals.parsedApnonceLen; i++) info("%02x",*tmp++);
            info("\n");
        }else{
            reterror(-7, "[TSSC] manually specified ApNonce=%s, but parsing failed\n",apnonce);
        }
    }
    
    if (sepnonce) {
        if ((devVals.sepnonce = parseNonce(sepnonce,&devVals.parsedSepnonceLen)) ){
            info("[TSSC] manually specified sepnonce to use, parsed \"%s\" to hex:",sepnonce);
            unsigned char *tmp = (unsigned char*)devVals.sepnonce;
            for (int i=0; i< devVals.parsedSepnonceLen; i++) info("%02x",*tmp++);
            info("\n");
        }else{
            reterror(-7, "[TSSC] manually specified SepNonce=%s, but parsing failed\n",sepnonce);
        }
    }
    
    if (bbsnum) {
        t_bbdevice bbinfo = getBBDeviceInfo(devVals.deviceModel);
        if (bbinfo->bbsnumSize == 0) {
            reterror(-8, "[TSSC] this device has no baseband, so it does not make sense to provide BbSNUM.\n");
        }
        
        if ((devVals.bbsnum = (uint8_t *)parseNonce(bbsnum, &devVals.bbsnumSize))) {
            info("[TSSC] manually specified BbSNUM to use, parsed \"%s\" to hex:", bbsnum);
            unsigned char *tmp = devVals.bbsnum;
            for (int i=0; i< devVals.bbsnumSize; i++) info("%02x", *tmp++);
            info("\n");
            
            if (bbinfo->bbsnumSize != devVals.bbsnumSize) {
                reterror(-8, "[TSSC] BbSNUM length for this device should be %d, but you gave one of length %d\n", (int)bbinfo->bbsnumSize,
                         (int)devVals.bbsnumSize);
            }
        } else {
            reterror(-7, "[TSSC] manually specified BbSNUM=%s, but parsing failed\n", bbsnum);
        }
    }
    
    if (!buildmanifest) { //no need to get firmwares/ota json if specifying buildmanifest manually
    reparse:
        firmwareJson = (versVals.isOta) ? getOtaJson() : getFirmwareJson();
        if (!devVals.installType) //only set this if installType wasn't set manually
            devVals.isUpdateInstall = (versVals.isOta); //there are no erase installs over OTA
        if (!firmwareJson) reterror(-6,"[TSSC] could not get firmware.json\n");
        
        long cnt = parseTokens(firmwareJson, &firmwareTokens);
        if (cnt < 1){
            if (!nocache){
                warning("[TSSC] error parsing cached %s.json. Trying to redownload\n",(versVals.isOta) ? "ota" : "firmware");
                nocache = 1;
                goto reparse; //in case cached json is corrupt, try to redownload and reparse one more time.
            }
            reterror(-2,"[TSSC] parsing %s.json failed\n",(versVals.isOta) ? "ota" : "firmware");
        }
    }

    if (flags & FLAG_LATEST_IOS && !versVals.version) {
        int versionCnt = 0;
        int i = 0;
            
        char **versions = getListOfiOSForDevice(firmwareTokens, devVals.deviceModel, versVals.isOta, &versionCnt);
        if (!versionCnt) reterror(-8, "[TSSC] failed finding latest firmware version. If you using --boardconfig, please also specify device model with -d ota=%d\n",versVals.isOta);
        char *bpos = NULL;
        while((bpos = strstr(versVals.version = strdup(versions[i++]),"[B]")) != 0){
            if (versVals.useBeta) break;
            free((char*)versVals.version);
            if (--versionCnt == 0) reterror(-9, "[TSSC] automatic firmware selection couldn't find non-beta firmware\n");
        }
        info("[TSSC] selecting latest version of firmware: %s\n",versVals.version);
        if (bpos) *bpos= '\0';
        if (versions) free(versions[versionCnt-1]),free(versions);
    }
    
    if (flags & FLAG_LIST_DEVICES) {
        printListOfDevices(firmwareTokens);
    }else if (flags & FLAG_LIST_IOS){
        if (!devVals.deviceModel)
            reterror(-3,"[TSSC] please specify a device for this option\n\tuse -h for more help\n");

        printListOfiOSForDevice(firmwareTokens, devVals.deviceModel, versVals.isOta);
    }else{
        //request ticket
        if (buildmanifest) {
            isSigned = isManifestSignedForDevice(buildmanifest, &devVals, &versVals);

        }else{
            if (!devVals.deviceModel) reterror(-3,"[TSSC] please specify a device for this option\n\tuse -h for more help\n");
            if (!versVals.version && !versVals.buildID) reterror(-5,"[TSSC] please specify an firmware version or buildID for this option\n\tuse -h for more help\n");
            
            isSigned = isVersionSignedForDevice(firmwareTokens, &versVals, &devVals);
        }
        
        if (isSigned >=0) printf("\n%s %s for device %s %s being signed!\n",(versVals.buildID) ? "Build" : "iOS" ,(versVals.buildID ? versVals.buildID : versVals.version),devVals.deviceModel, (isSigned) ? "IS" : "IS NOT");
        else{
            putchar('\n');
            reterror(-69, "[TSSC] checking tss status failed!\n");
        }
    }
    
error:
    if (devVals.deviceBoard) free(devVals.deviceBoard);
    if (devVals.deviceModel) free(devVals.deviceModel);
    if (devVals.apnonce) free(devVals.apnonce);
    if (devVals.sepnonce) free(devVals.sepnonce);
    if (devVals.bbsnum) free(devVals.bbsnum);
    if (firmwareJson) free(firmwareJson);
    if (firmwareTokens) free(firmwareTokens);
    return err ? err : !isSigned;
#undef reterror
}
