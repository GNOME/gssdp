#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>

int
main ()
{
        PMIB_IPNET_TABLE2 pipTable = NULL;
        //    MIB_IPNET_ROW2 ipRow;

        unsigned long status = GetIpNetTable2 (AF_INET, &pipTable);
        if (status != NO_ERROR) {
                printf ("GetIpNetTable for IPv4 table returned error: %ld\n",
                        status);
        }
        FreeMibTable (pipTable);
}
