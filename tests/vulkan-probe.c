int _fltused=0;
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define VK_NO_PROTOTYPES
#include "wine/vulkan.h"
void *memset(void *p,int c,size_t n) {volatile unsigned char *b=p;for(size_t i=0;i<n;i++)b[i]=c;return p;}
static void print(const char *s) {DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static void pump(int n) {MSG m;for(int i=0;i<n;i++){while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(10);}}
static void fail(const char *s,int code) {print(s);ExitProcess(code);}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
static PFN_vkGetInstanceProcAddr gipa;
static VkInstance instance;
#define I(f) PFN_##f p##f=(PFN_##f)gipa(instance,#f)
/* Presents red frames to a window through a Win32 Vulkan swapchain. An X11
 * driver without presentation support on the X server exits with 3. */
void mainCRTStartup(void) {
 HINSTANCE module=GetModuleHandleA(0);WNDCLASSA wc={0};wc.lpfnWndProc=proc;wc.hInstance=module;wc.lpszClassName="VulkanProbe";RegisterClassA(&wc);
 HWND window=CreateWindowExA(0,wc.lpszClassName,"Vulkan probe",WS_POPUP|WS_VISIBLE,0,0,320,240,0,0,module,0);if(!window)fail("window-failed\n",1);pump(50);
 HMODULE vulkan=LoadLibraryA("vulkan-1.dll");if(!vulkan)fail("no-vulkan-1\n",1);gipa=(PFN_vkGetInstanceProcAddr)(void(*)(void))GetProcAddress(vulkan,"vkGetInstanceProcAddr");
 const char *iext[]={"VK_KHR_surface","VK_KHR_win32_surface"};VkApplicationInfo app={VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_1;
 VkInstanceCreateInfo ici={VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;ici.enabledExtensionCount=2;ici.ppEnabledExtensionNames=iext;
 PFN_vkCreateInstance pvkCreateInstance=(PFN_vkCreateInstance)gipa(0,"vkCreateInstance");if(pvkCreateInstance(&ici,0,&instance))fail("instance-failed\n",1);
 I(vkEnumeratePhysicalDevices);I(vkCreateWin32SurfaceKHR);I(vkGetPhysicalDeviceSurfaceSupportKHR);I(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);I(vkGetPhysicalDeviceSurfaceFormatsKHR);I(vkCreateDevice);I(vkGetDeviceProcAddr);
 VkPhysicalDevice gpu;uint32_t count=1;if(pvkEnumeratePhysicalDevices(instance,&count,&gpu)<0||!count)fail("no-device\n",1);
 VkWin32SurfaceCreateInfoKHR sci={VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};sci.hinstance=module;sci.hwnd=window;VkSurfaceKHR surface;if(pvkCreateWin32SurfaceKHR(instance,&sci,0,&surface))fail("surface-failed\n",2);
 VkBool32 supported=0;pvkGetPhysicalDeviceSurfaceSupportKHR(gpu,0,surface,&supported);if(!supported)fail("no-presentation-support\n",3);print("presentation-supported\n");
 float priority=1;VkDeviceQueueCreateInfo qci={VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueCount=1;qci.pQueuePriorities=&priority;const char *dext[]={"VK_KHR_swapchain"};
 VkDeviceCreateInfo dci={VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;dci.enabledExtensionCount=1;dci.ppEnabledExtensionNames=dext;VkDevice device;if(pvkCreateDevice(gpu,&dci,0,&device))fail("device-failed\n",1);
#define D(f) PFN_##f p##f=(PFN_##f)pvkGetDeviceProcAddr(device,#f)
 D(vkGetDeviceQueue);D(vkCreateSwapchainKHR);D(vkGetSwapchainImagesKHR);D(vkAcquireNextImageKHR);D(vkQueuePresentKHR);D(vkCreateCommandPool);D(vkAllocateCommandBuffers);D(vkBeginCommandBuffer);D(vkEndCommandBuffer);D(vkCmdPipelineBarrier);D(vkCmdClearColorImage);D(vkQueueSubmit);D(vkCreateFence);D(vkWaitForFences);D(vkResetFences);D(vkResetCommandBuffer);D(vkDeviceWaitIdle);
 VkQueue queue;pvkGetDeviceQueue(device,0,0,&queue);VkSurfaceCapabilitiesKHR caps;pvkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu,surface,&caps);
 VkSurfaceFormatKHR formats[64];count=64;pvkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&count,formats);VkSurfaceFormatKHR format=formats[0];for(uint32_t i=0;i<count;i++)if(formats[i].format==VK_FORMAT_B8G8R8A8_UNORM)format=formats[i];
 VkSwapchainCreateInfoKHR swci={VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};swci.surface=surface;swci.minImageCount=caps.minImageCount<2?2:caps.minImageCount;swci.imageFormat=format.format;swci.imageColorSpace=format.colorSpace;
 swci.imageExtent=caps.currentExtent.width==0xffffffff?(VkExtent2D){320,240}:caps.currentExtent;swci.imageArrayLayers=1;swci.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;swci.preTransform=caps.currentTransform;swci.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;swci.presentMode=VK_PRESENT_MODE_FIFO_KHR;swci.clipped=1;
 VkSwapchainKHR swapchain;if(pvkCreateSwapchainKHR(device,&swci,0,&swapchain))fail("swapchain-failed\n",4);
 VkImage images[16];count=16;pvkGetSwapchainImagesKHR(device,swapchain,&count,images);
 VkCommandPoolCreateInfo cpci={VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;VkCommandPool pool;pvkCreateCommandPool(device,&cpci,0,&pool);
 VkCommandBufferAllocateInfo cbai={VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cbai.commandPool=pool;cbai.commandBufferCount=1;VkCommandBuffer cb;pvkAllocateCommandBuffers(device,&cbai,&cb);
 VkFenceCreateInfo fci={VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence acquired,done;pvkCreateFence(device,&fci,0,&acquired);pvkCreateFence(device,&fci,0,&done);
 VkClearColorValue red={{1.0f,0.05f,0.1f,1.0f}};VkImageSubresourceRange range={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
 for(int frame=0;frame<60;frame++){
  uint32_t index;if(pvkAcquireNextImageKHR(device,swapchain,2000000000ull,0,acquired,&index)<0)fail("acquire-failed\n",5);pvkWaitForFences(device,1,&acquired,1,~0ull);pvkResetFences(device,1,&acquired);
  pvkResetCommandBuffer(cb,0);VkCommandBufferBeginInfo cbbi={VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};pvkBeginCommandBuffer(cb,&cbbi);
  VkImageMemoryBarrier b={VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=images[index];b.subresourceRange=range;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
  pvkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,0,0,0,1,&b);pvkCmdClearColorImage(cb,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&red,1,&range);
  b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=0;pvkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,0,0,0,1,&b);pvkEndCommandBuffer(cb);
  VkSubmitInfo si={VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cb;if(pvkQueueSubmit(queue,1,&si,done))fail("submit-failed\n",5);pvkWaitForFences(device,1,&done,1,~0ull);pvkResetFences(device,1,&done);
  VkPresentInfoKHR pi={VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};pi.swapchainCount=1;pi.pSwapchains=&swapchain;pi.pImageIndices=&index;if(pvkQueuePresentKHR(queue,&pi)<0)fail("present-failed\n",6);pump(1);
 }
 pvkDeviceWaitIdle(device);print("presented\n");pump(200);ExitProcess(0);
}
