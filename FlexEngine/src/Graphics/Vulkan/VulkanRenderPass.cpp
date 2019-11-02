#include "stdafx.hpp"
#if COMPILE_VULKAN

#include "Graphics/Vulkan/VulkanRenderPass.hpp"

#include "Graphics/Vulkan/VulkanDevice.hpp"
#include "Graphics/Vulkan/VulkanHelpers.hpp"
#include "Graphics/Vulkan/VulkanInitializers.hpp"
#include "Graphics/Vulkan/VulkanRenderer.hpp"

namespace flex
{
	namespace vk
	{
		VulkanRenderPass::VulkanRenderPass()
		{
		}

		VulkanRenderPass::VulkanRenderPass(VulkanDevice* device) :
			m_RenderPass{ device->m_LogicalDevice, vkDestroyRenderPass },
			m_VulkanDevice(device)
		{
		}

		void VulkanRenderPass::Create(
			const char* passName,
			VkRenderPassCreateInfo* createInfo,
			const std::vector<FrameBufferID>& inTargetFrameBufferIDs,
			const std::vector<FrameBufferID>& inSampledFrameBufferIDs)
		{
			targetFrameBufferIDs = inTargetFrameBufferIDs;
			sampledFrameBufferIDs = inSampledFrameBufferIDs;
			m_Name = passName;

			VK_CHECK_RESULT(vkCreateRenderPass(m_VulkanDevice->m_LogicalDevice, createInfo, nullptr, m_RenderPass.replace()));
			((VulkanRenderer*)g_Renderer)->SetRenderPassName(m_VulkanDevice, m_RenderPass, passName);
		}

		void VulkanRenderPass::CreateColorAndDepth(
			const char* passName,
			VkFormat colorFormat,
			FrameBufferID targetFrameBufferID,
			const std::vector<FrameBufferID>& inSampledFrameBufferIDs,
			VkImageLayout finalLayout /* = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL */,
			VkImageLayout initialLayout /* = VK_IMAGE_LAYOUT_UNDEFINED */,
			VkFormat depthFormat /* = VK_FORMAT_UNDEFINED */,
			VkImageLayout finalDepthLayout /* = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL */,
			VkImageLayout initialDepthLayout /* = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL */)
		{
			VkAttachmentDescription colorAttachment = vks::attachmentDescription(colorFormat, finalLayout);
			VkAttachmentDescription depthAttachment = vks::attachmentDescription(depthFormat, finalDepthLayout);

			if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
			{
				colorAttachment.initialLayout = initialLayout;
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

				depthAttachment.initialLayout = initialDepthLayout;
				depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			}

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference depthAttachmentRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			Create(passName, { targetFrameBufferID }, inSampledFrameBufferIDs,
				&colorAttachment, &colorReference, 1,
				&depthAttachment, &depthAttachmentRef);
		}

		void VulkanRenderPass::CreateMultiColorAndDepth(
			const char* passName,
			FrameBufferID targetFrameBufferID,
			const std::vector<FrameBufferID>& inSampledFrameBufferIDs,
			VkFormat* colorFormats,
			u32 colorAttachmentCount,
			VkFormat depthFormat)
		{
			std::vector<VkAttachmentDescription> colorAttachments(colorAttachmentCount);
			for (u32 i = 0; i < colorAttachmentCount; ++i)
			{
				colorAttachments[i] = vks::attachmentDescription(colorFormats[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
			VkAttachmentDescription depthAttachment = vks::attachmentDescription(depthFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			std::vector<VkAttachmentReference> colorReferences(colorAttachmentCount);
			for (u32 i = 0; i < colorAttachmentCount; ++i)
			{
				colorReferences[i] = VkAttachmentReference{ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			}
			VkAttachmentReference depthAttachmentRef = { colorAttachmentCount, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			Create(passName, { targetFrameBufferID }, inSampledFrameBufferIDs,
				colorAttachments.data(), colorReferences.data(), colorAttachmentCount,
				&depthAttachment, &depthAttachmentRef);
		}

		void VulkanRenderPass::CreateDepthOnly(
			const char* passName,
			FrameBufferID targetFrameBufferID,
			const std::vector<FrameBufferID>& inSampledFrameBufferIDs,
			VkFormat depthFormat /* = VK_FORMAT_UNDEFINED */,
			VkImageLayout finalDepthLayout /* = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL */,
			VkImageLayout initialDepthLayout /* = VK_IMAGE_LAYOUT_UNDEFINED */)
		{
			VkAttachmentDescription depthAttachment = vks::attachmentDescription(depthFormat, finalDepthLayout);

			if (initialDepthLayout != VK_IMAGE_LAYOUT_UNDEFINED)
			{
				depthAttachment.initialLayout = initialDepthLayout;
				depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			}

			VkAttachmentReference depthAttachmentRef = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			Create(passName, { targetFrameBufferID }, inSampledFrameBufferIDs,
				nullptr, nullptr, 0,
				&depthAttachment, &depthAttachmentRef);
		}

		void VulkanRenderPass::CreateColorOnly(
			const char* passName,
			VkFormat colorFormat,
			FrameBufferID targetFrameBufferID,
			const std::vector<FrameBufferID>& inSampledFrameBufferIDs,
			VkImageLayout finalLayout /* = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL */,
			VkImageLayout initialLayout /* = VK_IMAGE_LAYOUT_UNDEFINED */)
		{
			VkAttachmentDescription colorAttachment = vks::attachmentDescription(colorFormat, finalLayout);

			if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
			{
				colorAttachment.initialLayout = initialLayout;
				colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			}

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			Create(passName, { targetFrameBufferID }, inSampledFrameBufferIDs,
				&colorAttachment, &colorReference, 1,
				nullptr, nullptr);
		}

		void VulkanRenderPass::Create(
			const char* passName,
			const std::vector<FrameBufferID>& inTargetFrameBufferIDs,
			const std::vector<FrameBufferID>& inSampledFrameBufferIDs,
			VkAttachmentDescription* colorAttachments,
			VkAttachmentReference* colorAttachmentReferences,
			u32 colorAttachmentCount,
			VkAttachmentDescription* depthAttachment,
			VkAttachmentReference* depthAttachmentRef)
		{
			assert(
				(colorAttachmentCount == 0 && colorAttachments == nullptr && colorAttachmentReferences == nullptr) ||
				(colorAttachmentCount != 0 && colorAttachments != nullptr && colorAttachmentReferences != nullptr));
			assert((depthAttachment != nullptr) == (depthAttachmentRef != nullptr));

			std::vector<VkAttachmentDescription> attachments(colorAttachmentCount + (depthAttachment != nullptr ? 1 : 0));

			for (u32 i = 0; i < colorAttachmentCount; ++i)
			{
				attachments[i] = *colorAttachments;
			}

			if (depthAttachment)
			{
				attachments[attachments.size() - 1] = *depthAttachment;
			}

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = colorAttachmentCount;
			subpass.pColorAttachments = colorAttachmentReferences;
			subpass.pDepthStencilAttachment = depthAttachmentRef ? depthAttachmentRef : nullptr;

			std::array<VkSubpassDependency, 2> dependencies;
			dependencies[0] = {};
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1] = {};
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassCreateInfo = vks::renderPassCreateInfo();
			renderPassCreateInfo.attachmentCount = attachments.size();
			renderPassCreateInfo.pAttachments = attachments.data();
			renderPassCreateInfo.subpassCount = 1;
			renderPassCreateInfo.pSubpasses = &subpass;
			renderPassCreateInfo.dependencyCount = dependencies.size();
			renderPassCreateInfo.pDependencies = dependencies.data();

			Create(passName, &renderPassCreateInfo, inTargetFrameBufferIDs, inSampledFrameBufferIDs);
		}

		void VulkanRenderPass::Begin_WithFrameBuffer(VkCommandBuffer cmdBuf, VkClearValue* clearValues, u32 clearValueCount, FrameBufferID targetFrameBufferID)
		{
			Begin(cmdBuf, clearValues, clearValueCount, targetFrameBufferID);
		}

		void VulkanRenderPass::Begin(VkCommandBuffer cmdBuf, VkClearValue* clearValues, u32 clearValueCount)
		{
			Begin(cmdBuf, clearValues, clearValueCount, targetFrameBufferIDs[0]);
		}

		void VulkanRenderPass::Begin(VkCommandBuffer cmdBuf, VkClearValue* clearValues, u32 clearValueCount, FrameBufferID targetFrameBufferID)
		{
			if (m_ActiveCommandBuffer != VK_NULL_HANDLE)
			{
				PrintError("Attempted to begin render pass (%s) multiple times! Did you forget to call End?\n", m_Name);
				return;
			}

			m_ActiveCommandBuffer = cmdBuf;

			FrameBuffer* frameBuffer = ((VulkanRenderer*)g_Renderer)->GetFrameBuffer(targetFrameBufferID);

			VkRenderPassBeginInfo renderPassBeginInfo = vks::renderPassBeginInfo(m_RenderPass);

			renderPassBeginInfo.renderPass = m_RenderPass;
			renderPassBeginInfo.framebuffer = frameBuffer->frameBuffer;
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = { frameBuffer->width, frameBuffer->height };
			renderPassBeginInfo.clearValueCount = clearValueCount;
			renderPassBeginInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		void VulkanRenderPass::End()
		{
			if (m_ActiveCommandBuffer == VK_NULL_HANDLE)
			{
				PrintError("Attempted to end render pass which has invalid m_ActiveCommandBuffer (%s)", m_Name);
				return;
			}

			vkCmdEndRenderPass(m_ActiveCommandBuffer);

			m_ActiveCommandBuffer = VK_NULL_HANDLE;
		}

		VkRenderPass* VulkanRenderPass::Replace()
		{
			return m_RenderPass.replace();
		}

		VulkanRenderPass::operator VkRenderPass()
		{
			return m_RenderPass;
		}
	} // namespace vk
} // namespace flex

#endif // COMPILE_VULKAN