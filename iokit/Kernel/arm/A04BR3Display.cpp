/* Board display provider for the experimental CPU IOMobileFramebuffer path.
 * Physical panel/DSI initialization remains in bsd/dev/a04br3_display.c.
 */
#include <pexpert/arm64/board_config.h>
#if defined(A04BR3)
#include <IOKit/IOService.h>
#include <pexpert/pexpert.h>
class A04BR3Display : public IOService {
    OSDeclareDefaultStructors(A04BR3Display);
public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(A04BR3Display, IOService);
bool A04BR3Display::start(IOService *provider)
{
    unsigned enabled = 0;
    if (!PE_parse_boot_argn("a04br3.display_probe", &enabled, sizeof(enabled)) || enabled != 1) return false;
    if (!IOService::start(provider)) return false;
    uint32_t width = 800, height = 1280;
    setProperty("canvas-width", &width, sizeof(width));
    setProperty("canvas-height", &height, sizeof(height));
    setProperty("A04BR3DevelopmentOnly", kOSBooleanTrue);
    registerService();
    return true;
}
#endif
