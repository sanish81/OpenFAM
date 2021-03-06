/*
 * fam_invalid_key_test.cpp
 * Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
 * reserved. Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */
#include <iostream>
#include <stdio.h>
#include <string.h>

#include <fam/fam.h>

#include "common/fam_libfabric.h"
#include "common/fam_ops_libfabric.h"

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;
    Fam_Descriptor *item;
    Fam_Global_Descriptor global;

    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    int ret = 0;
    int pass = 0, fail = 0;

    char *buff = (char *)malloc(1024);
    memset(buff, 0, 1024);

    // Initialize the libfabric ops
    char *message = strdup("This is the datapath test");
    char *name = strdup("127.0.0.1");
    char *service = strdup(TEST_LIBFABRIC_PORT);
    char *provider = strdup(TEST_LIBFABRIC_PROVIDER);
    std::map <uint64_t, string> nameMap;
    nameMap.insert({0, name});
    // initialize gRPC client
    Fam_Allocator_Grpc *famAllocator =
        new Fam_Allocator_Grpc(nameMap, atoi(TEST_GRPC_PORT));

    Fam_Ops_Libfabric *famOps = new Fam_Ops_Libfabric(
        name, service, false, provider, FAM_THREAD_MULTIPLE, famAllocator,
        FAM_CONTEXT_DEFAULT);

    // ret = famOps->initialize(name, service, false, provider);
    ret = famOps->initialize();
    if (ret < 0) {
        cout << "famOps initialization failed" << endl;
        exit(1);
    }

    // Initialize openFAM API
    fam_opts.memoryServer = strdup("0:127.0.0.1");
    fam_opts.grpcPort = strdup(TEST_GRPC_PORT);
    if (my_fam->fam_initialize("default", &fam_opts) < 0) {
        cout << "fam initialization failed" << endl;
        exit(1);
    } else {
        cout << "fam initialization successful" << endl;
    }

    // Create Region
    desc = my_fam->fam_create_region("test", 8192, 0777, RAID1);
    if (desc == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    } else {
        global = desc->get_global_descriptor();
        cout << " Fam_Region_Descriptor { Region ID : 0x" << hex << uppercase
             << global.regionId << ", Offset : 0x" << global.offset << " }"
             << endl;
    }

    // Allocating data items in the created region
    item = my_fam->fam_allocate("first", 1024, 0777, desc);
    if (item == NULL) {
        cout << "fam allocation of dataitem 'first' failed" << endl;
    } else {
        global = item->get_global_descriptor();
        cout << " Fam_Descriptor { Region ID : 0x" << hex << uppercase
             << global.regionId << ", Offset : 0x" << global.offset
             << ", Key : 0x" << item->get_key() << " }" << endl;
    }

    uint64_t nodeId = item->get_memserver_id();

    // Write with valid key
    try {
        ret =
            fabric_write(item->get_key(), message, 25, 0,
                         (*famOps->get_fiAddrs())[nodeId], famOps->get_defaultCtx(item));
        if (ret < 0) {
            cout << "fabric write failed" << endl;
            exit(1);
        }
    } catch (Fam_Permission_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        fail++;
    } catch (Fam_Datapath_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        fail++;
    }

    // Read with valid key
    try {
        ret =
            fabric_read(item->get_key(), buff, 25, 0,
                        (*famOps->get_fiAddrs())[nodeId], famOps->get_defaultCtx(item));
        if (ret < 0) {
            cout << "fabric read failed" << endl;
            exit(1);
        }
    } catch (Fam_Permission_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        fail++;
    } catch (Fam_Datapath_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        fail++;
    }

    cout << "With valid key : " << endl;
    cout << "key = " << item->get_key() << endl;
    cout << "buffer = " << buff << endl;
    if (strncmp(message, buff, 25) == 0) {
        cout << "Data read is same is as written" << endl;
        pass++;
    } else {
        cout << "Read and Written Data are different" << endl;
        fail++;
    }

    uint64_t invalidKey = -1;
    // Write with invalid key
    try {
        ret =
            fabric_write(invalidKey, message, 25, 0,
                         (*famOps->get_fiAddrs())[nodeId], famOps->get_defaultCtx(item));

        if (ret < 0) {
            cout << "fabric write failed" << endl;
            exit(1);
        }
    } catch (Fam_Datapath_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        pass++;
    }

    memset(buff, 0, 1024);
    // Read with invalid key
    try {
        ret = fabric_read(invalidKey, buff, 25, 0, (*famOps->get_fiAddrs())[nodeId],
                          famOps->get_defaultCtx(item));

        if (ret < 0) {
            cout << "fabric read failed" << endl;
            exit(1);
        }
    } catch (Fam_Datapath_Exception &e) {
        cout << "Exception caught" << endl;
        cout << "Error msg: " << e.fam_error_msg() << endl;
        cout << "Error: " << e.fam_error() << endl;
        pass++;
    }

    cout << "With invalid key : " << endl;
    cout << "key = " << invalidKey << endl;
    cout << "buffer = " << buff << endl;

    // Deallocating data items
    if (item != NULL)
        my_fam->fam_deallocate(item);

    // Destroying the regioin
    if (desc != NULL)
        my_fam->fam_destroy_region(desc);

    famOps->finalize();
    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;

    if (fail) {
        cout << "Test failed. fail=" << fail << endl;
        return -1;
    } else {
        cout << "Test passed. pass=" << pass << endl;
        return 0;
    }
}
