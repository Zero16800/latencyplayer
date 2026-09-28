/*
 * Stubs for symbols referenced by GStreamer static libraries
 * that don't exist in Android Bionic, plus Vulkan entry points
 * pulled in by libgstgl (appsink path does not use Vulkan).
 *
 * Return VK_ERROR_INITIALIZATION_FAILED (-3) so callers fail cleanly.
 */

#include <stddef.h>
#include <stdint.h>

/* GLib references __gnu_strerror_r from glibc */
int __gnu_strerror_r(int errnum, char *buf, unsigned long buflen)
{
    (void)errnum;
    if (buf && buflen > 0) buf[0] = '\0';
    return 0;
}

/* Bionic lacks these glibc/libc symbols referenced by static glib/gio.
 * Do NOT include <netinet/in.h> here — it declares these as static. */
#include <grp.h>

typedef struct { unsigned char s6_addr[16]; } in6_addr_t_stub;
const in6_addr_t_stub in6addr_any = { .s6_addr = {0} };
const in6_addr_t_stub in6addr_loopback = {
    .s6_addr = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}
};

int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen, struct group **result)
{
    (void)gid; (void)grp; (void)buf; (void)buflen;
    if (result) *result = NULL;
    return 0;
}

/* 32-bit ABIs: glibc-style *64 stdio names referenced by static libs */
#include <stdio.h>
#include <sys/types.h>

int fseeko64(FILE *stream, off64_t offset, int whence)
{
    return fseeko(stream, (off_t)offset, whence);
}

off64_t ftello64(FILE *stream)
{
    return (off64_t)ftello(stream);
}

/* ---- Vulkan stubs (no libvulkan at link time; minSdk 21 has no libvulkan) ---- */
#define VK_SUCCESS 0
#define VK_ERROR_INITIALIZATION_FAILED (-3)
#define VK_INCOMPLETE 5
#define VKAPI_CALL
#define VKAPI_PTR

typedef int32_t VkResult;
typedef uint32_t VkBool32;
typedef uint32_t VkFlags;
typedef uint32_t VkStructureType;
typedef uint64_t VkDeviceSize;
typedef void *VkInstance;
typedef void *VkPhysicalDevice;
typedef void *VkDevice;
typedef void *VkQueue;
typedef void *VkBuffer;
typedef void *VkCommandBuffer;
typedef void *VkImage;
typedef void *VkImageView;
typedef void *VkFramebuffer;
typedef void *VkRenderPass;
typedef void *VkPipeline;
typedef void *VkPipelineLayout;
typedef void *VkShaderModule;
typedef void *VkSampler;
typedef void *VkSemaphore;
typedef void *VkFence;
typedef void *VkDeviceMemory;
typedef void *VkCommandPool;
typedef void *VkDescriptorPool;
typedef void *VkDescriptorSetLayout;
typedef void *VkDescriptorSet;
typedef void *VkQueryPool;
typedef void *VkPipelineCache;
typedef struct VkPhysicalDeviceProperties VkPhysicalDeviceProperties;
typedef struct VkPhysicalDeviceMemoryProperties VkPhysicalDeviceMemoryProperties;
typedef struct VkPhysicalDeviceFeatures VkPhysicalDeviceFeatures;
typedef struct VkFormatProperties VkFormatProperties;
typedef struct VkImageFormatProperties VkImageFormatProperties;
typedef struct VkQueueFamilyProperties VkQueueFamilyProperties;
typedef struct VkExtensionProperties VkExtensionProperties;
typedef struct VkLayerProperties VkLayerProperties;
typedef struct VkMemoryRequirements VkMemoryRequirements;
typedef struct VkExtent3D VkExtent3D;

typedef void (*PFN_vkVoidFunction)(void);

#define VK_STUB_RET_FAIL VkResult: return VK_ERROR_INITIALIZATION_FAILED
#define VK_STUB_PTR(name, T) void *name(void) { (void)0; return NULL; }

VkResult VKAPI_CALL vkCreateInstance(const void *pCreateInfo, const void *pAllocator, VkInstance *pInstance)
{
    (void)pCreateInfo; (void)pAllocator;
    if (pInstance) *pInstance = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyInstance(VkInstance instance, const void *pAllocator)
{
    (void)instance; (void)pAllocator;
}

VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pCount, VkPhysicalDevice *pDevices)
{
    (void)instance; (void)pDevices;
    if (pCount) *pCount = 0;
    return VK_INCOMPLETE;
}

void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties *pProperties)
{
    (void)physicalDevice; (void)pProperties;
}

void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t *pCount, VkQueueFamilyProperties *pProperties)
{
    (void)physicalDevice; (void)pProperties;
    if (pCount) *pCount = 0;
}

void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
    (void)physicalDevice; (void)pMemoryProperties;
}

void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *pFeatures)
{
    (void)physicalDevice; (void)pFeatures;
}

void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, uint32_t format, VkFormatProperties *pFormatProperties)
{
    (void)physicalDevice; (void)format; (void)pFormatProperties;
}

VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, uint32_t format, uint32_t type, uint32_t tiling, uint32_t usage, uint32_t flags, VkImageFormatProperties *pImageFormatProperties)
{
    (void)physicalDevice; (void)format; (void)type; (void)tiling; (void)usage; (void)flags; (void)pImageFormatProperties;
    return VK_ERROR_INITIALIZATION_FAILED;
}

PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
    (void)instance; (void)pName;
    return NULL;
}

PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
    (void)device; (void)pName;
    return NULL;
}

VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const void *pCreateInfo, const void *pAllocator, VkDevice *pDevice)
{
    (void)physicalDevice; (void)pCreateInfo; (void)pAllocator;
    if (pDevice) *pDevice = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyDevice(VkDevice device, const void *pAllocator)
{
    (void)device; (void)pAllocator;
}

void VKAPI_CALL vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue)
{
    (void)device; (void)queueFamilyIndex; (void)queueIndex;
    if (pQueue) *pQueue = NULL;
}

VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount, const void *pSubmits, VkFence fence)
{
    (void)queue; (void)submitCount; (void)pSubmits; (void)fence;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device)
{
    (void)device;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkAllocateMemory(VkDevice device, const void *pAllocateInfo, const void *pAllocator, VkDeviceMemory *pMemory)
{
    (void)device; (void)pAllocateInfo; (void)pAllocator;
    if (pMemory) *pMemory = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkFreeMemory(VkDevice device, VkDeviceMemory memory, const void *pAllocator)
{
    (void)device; (void)memory; (void)pAllocator;
}

VkResult VKAPI_CALL vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkFlags flags, void **ppData)
{
    (void)device; (void)memory; (void)offset; (void)size; (void)flags;
    if (ppData) *ppData = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
    (void)device; (void)memory;
}

VkResult VKAPI_CALL vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const void *pMemoryRanges)
{
    (void)device; (void)memoryRangeCount; (void)pMemoryRanges;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const void *pMemoryRanges)
{
    (void)device; (void)memoryRangeCount; (void)pMemoryRanges;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements *pMemoryRequirements)
{
    (void)device; (void)buffer; (void)pMemoryRequirements;
}

void VKAPI_CALL vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements *pMemoryRequirements)
{
    (void)device; (void)image; (void)pMemoryRequirements;
}

VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    (void)device; (void)buffer; (void)memory; (void)memoryOffset;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
    (void)device; (void)image; (void)memory; (void)memoryOffset;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkCreateBuffer(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkBuffer *pBuffer)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pBuffer) *pBuffer = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer, const void *pAllocator)
{
    (void)device; (void)buffer; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateImage(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkImage *pImage)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pImage) *pImage = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image, const void *pAllocator)
{
    (void)device; (void)image; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateImageView(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkImageView *pView)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pView) *pView = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView, const void *pAllocator)
{
    (void)device; (void)imageView; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkFramebuffer *pFramebuffer)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pFramebuffer) *pFramebuffer = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const void *pAllocator)
{
    (void)device; (void)framebuffer; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkRenderPass *pRenderPass)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pRenderPass) *pRenderPass = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const void *pAllocator)
{
    (void)device; (void)renderPass; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const void *pCreateInfos, const void *pAllocator, VkPipeline *pPipelines)
{
    (void)device; (void)pipelineCache; (void)createInfoCount; (void)pCreateInfos; (void)pAllocator;
    if (pPipelines) { for (uint32_t i = 0; i < createInfoCount; i++) pPipelines[i] = NULL; }
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const void *pAllocator)
{
    (void)device; (void)pipeline; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkPipelineLayout *pPipelineLayout)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pPipelineLayout) *pPipelineLayout = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const void *pAllocator)
{
    (void)device; (void)pipelineLayout; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkShaderModule *pShaderModule)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pShaderModule) *pShaderModule = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const void *pAllocator)
{
    (void)device; (void)shaderModule; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateSampler(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkSampler *pSampler)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pSampler) *pSampler = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroySampler(VkDevice device, VkSampler sampler, const void *pAllocator)
{
    (void)device; (void)sampler; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateSemaphore(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkSemaphore *pSemaphore)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pSemaphore) *pSemaphore = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const void *pAllocator)
{
    (void)device; (void)semaphore; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateFence(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkFence *pFence)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pFence) *pFence = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyFence(VkDevice device, VkFence fence, const void *pAllocator)
{
    (void)device; (void)fence; (void)pAllocator;
}

VkResult VKAPI_CALL vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll, uint64_t timeout)
{
    (void)device; (void)fenceCount; (void)pFences; (void)waitAll; (void)timeout;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences)
{
    (void)device; (void)fenceCount; (void)pFences;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkGetFenceStatus(VkDevice device, VkFence fence)
{
    (void)device; (void)fence;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkCommandPool *pCommandPool)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pCommandPool) *pCommandPool = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const void *pAllocator)
{
    (void)device; (void)commandPool; (void)pAllocator;
}

VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device, const void *pAllocateInfo, VkCommandBuffer *pCommandBuffers)
{
    (void)device; (void)pAllocateInfo;
    if (pCommandBuffers) pCommandBuffers[0] = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const void *pBeginInfo)
{
    (void)commandBuffer; (void)pBeginInfo;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
    (void)commandBuffer;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkFlags flags)
{
    (void)commandBuffer; (void)flags;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkDescriptorSetLayout *pSetLayout)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pSetLayout) *pSetLayout = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const void *pAllocator)
{
    (void)device; (void)descriptorSetLayout; (void)pAllocator;
}

VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkDescriptorPool *pDescriptorPool)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pDescriptorPool) *pDescriptorPool = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const void *pAllocator)
{
    (void)device; (void)descriptorPool; (void)pAllocator;
}

VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device, const void *pAllocateInfo, VkDescriptorSet *pDescriptorSets)
{
    (void)device; (void)pAllocateInfo;
    if (pDescriptorSets) pDescriptorSets[0] = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets)
{
    (void)device; (void)descriptorPool; (void)descriptorSetCount; (void)pDescriptorSets;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const void *pDescriptorWrites, uint32_t descriptorCopyCount, const void *pDescriptorCopies)
{
    (void)device; (void)descriptorWriteCount; (void)pDescriptorWrites; (void)descriptorCopyCount; (void)pDescriptorCopies;
}

VkResult VKAPI_CALL vkCreateQueryPool(VkDevice device, const void *pCreateInfo, const void *pAllocator, VkQueryPool *pQueryPool)
{
    (void)device; (void)pCreateInfo; (void)pAllocator;
    if (pQueryPool) *pQueryPool = NULL;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const void *pAllocator)
{
    (void)device; (void)queryPool; (void)pAllocator;
}

VkResult VKAPI_CALL vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *pData, VkDeviceSize stride, VkFlags flags)
{
    (void)device; (void)queryPool; (void)firstQuery; (void)queryCount; (void)dataSize; (void)pData; (void)stride; (void)flags;
    return VK_ERROR_INITIALIZATION_FAILED;
}

void VKAPI_CALL vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkFlags flags)
{
    (void)commandBuffer; (void)queryPool; (void)query; (void)flags;
}

void VKAPI_CALL vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query)
{
    (void)commandBuffer; (void)queryPool; (void)query;
}

void VKAPI_CALL vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount)
{
    (void)commandBuffer; (void)queryPool; (void)firstQuery; (void)queryCount;
}

void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const void *pRenderPassBegin, uint32_t contents)
{
    (void)commandBuffer; (void)pRenderPassBegin; (void)contents;
}

void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
    (void)commandBuffer;
}

void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer, uint32_t pipelineBindPoint, VkPipeline pipeline)
{
    (void)commandBuffer; (void)pipelineBindPoint; (void)pipeline;
}

void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets)
{
    (void)commandBuffer; (void)pipelineBindPoint; (void)layout; (void)firstSet; (void)descriptorSetCount; (void)pDescriptorSets; (void)dynamicOffsetCount; (void)pDynamicOffsets;
}

void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t indexType)
{
    (void)commandBuffer; (void)buffer; (void)offset; (void)indexType;
}

void VKAPI_CALL vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
{
    (void)commandBuffer; (void)firstBinding; (void)bindingCount; (void)pBuffers; (void)pOffsets;
}

void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    (void)commandBuffer; (void)indexCount; (void)instanceCount; (void)firstIndex; (void)vertexOffset; (void)firstInstance;
}

void VKAPI_CALL vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, uint32_t srcImageLayout, VkImage dstImage, uint32_t dstImageLayout, uint32_t regionCount, const void *pRegions, uint32_t filter)
{
    (void)commandBuffer; (void)srcImage; (void)srcImageLayout; (void)dstImage; (void)dstImageLayout; (void)regionCount; (void)pRegions; (void)filter;
}

void VKAPI_CALL vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, uint32_t imageLayout, const void *pColor, uint32_t rangeCount, const void *pRanges)
{
    (void)commandBuffer; (void)image; (void)imageLayout; (void)pColor; (void)rangeCount; (void)pRanges;
}

void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, uint32_t srcStageMask, uint32_t dstStageMask, uint32_t dependencyFlags, uint32_t memoryBarrierCount, const void *pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const void *pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const void *pImageMemoryBarriers)
{
    (void)commandBuffer; (void)srcStageMask; (void)dstStageMask; (void)dependencyFlags; (void)memoryBarrierCount; (void)pMemoryBarriers; (void)bufferMemoryBarrierCount; (void)pBufferMemoryBarriers; (void)imageMemoryBarrierCount; (void)pImageMemoryBarriers;
}

VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
    (void)pLayerName; (void)pProperties;
    if (pPropertyCount) *pPropertyCount = 0;
    return VK_SUCCESS;
}

VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
    (void)pProperties;
    if (pPropertyCount) *pPropertyCount = 0;
    return VK_SUCCESS;
}

VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
    (void)physicalDevice; (void)pLayerName; (void)pProperties;
    if (pPropertyCount) *pPropertyCount = 0;
    return VK_SUCCESS;
}

VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
    (void)physicalDevice; (void)pProperties;
    if (pPropertyCount) *pPropertyCount = 0;
    return VK_SUCCESS;
}
