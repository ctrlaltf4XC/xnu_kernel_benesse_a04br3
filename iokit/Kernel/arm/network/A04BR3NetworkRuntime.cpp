/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "A04BR3NetworkRuntime.h"
#include "apple/IONetworkStack.h"
#include <sys/errno.h>
/* Board startup is serialized and only one MSDC2 radio exists. Apple's stack
 * itself rejects a second singleton. Keep a real stack reference for the boot,
 * rather than manufacturing a resource or a successful naming notification. */
static IONetworkStack *a04Stack;
A04BR3EthernetController *a04br3_network_start(IOService *provider,
    const a04_network_backend *backend,int *error)
{
    if(error)*error=EINVAL;
    if(!provider || !backend || !error)return nullptr;
    auto *controller=new A04BR3EthernetController;
    if(!controller){*error=ENOMEM;return nullptr;}
    if(!controller->init(nullptr)){controller->release();*error=ENOMEM;return nullptr;}
    if(!controller->configureBackend(backend)){controller->release();*error=ENXIO;return nullptr;}
    if(!a04Stack){
        auto *stack=new IONetworkStack;
        if(!stack){controller->release();*error=ENOMEM;return nullptr;}
        if(!stack->init(nullptr)){stack->release();controller->release();*error=ENOMEM;return nullptr;}
        if(!stack->attach(provider)){stack->release();controller->release();*error=ENXIO;return nullptr;}
        if(!stack->start(provider)){
            stack->detach(provider);stack->release();controller->release();*error=EIO;return nullptr;
        }
        a04Stack=stack;
    }
    if(!controller->attach(provider)){controller->release();*error=ENXIO;return nullptr;}
    if(!controller->start(provider)){
        controller->detach(provider);controller->release();*error=EIO;return nullptr;
    }
    IOReturn registered=a04Stack->registerKernelInterface(controller->networkInterface(),0);
    if(registered!=kIOReturnSuccess || !controller->networkInterface()->getIfnet()){
        IOLog("A04BR3 network: Stack registration failed result=0x%x; no live BSD interface\n",(unsigned)registered);
        controller->stop(provider);controller->detach(provider);controller->release();
        *error=EIO;return nullptr;
    }
    // Success now requires the actual Stack reserve/open/attach path and a
    // live shared ifnet, not merely controller/nub allocation. No fake state.
    IOLog("A04BR3 network: Stack registration complete; real shared BSD interface attached\n");
    *error=0;
    return controller;
}
