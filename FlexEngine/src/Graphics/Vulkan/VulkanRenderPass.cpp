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
		VulkanRenderPass::VulkanRenderPass(VulkanDevice* device) :
			m_VulkanDevice(device),
			m_RenderPass{ device->m_LogicalDevice, vkDestroyRenderPass }
		{
			m_FrameBuffer = new FrameBuffer(device);
		}

		VulkanRenderPass::~VulkanRenderPass()
		{
			if (!bCreateFrameBuffer)
			{
				// Prevent cleanup on framebuffers which we don't own
				m_FrameBuffer->frameBuffer = VK_NULL_HANDLE;
			}
			delete m_FrameBuffer;
		}

		void VulkanRenderPass::Create()
		{
			assert(m_bRegistered); // This function can only be called on render passes whose Register[...] function has been called

			const bool bDepthAttachmentPresent = m_TargetDepthAttachmentID != InvalidFrameBufferAttachmentID;

			const u32 colourAttachmentCount = (u32)m_TargetColourAttachmentIDs.size();
			const u32 attachmentCount = colourAttachmentCount + (bDepthAttachmentPresent ? 1 : 0);
			i32 frameBufferWidth = -1;
			i32 frameBufferHeight = -1;

			std::vector<VkAttachmentDescription> colourAttachments(colourAttachmentCount);
			std::vector<VkAttachmentReference> colourAttachmentReferences(colourAttachmentCount);
			std::vector<VkImageView> attachmentImageViews(attachmentCount);
			for (u32 i = 0; i < colourAttachmentCount; ++i)
			{
				FrameBufferAttachment* attachment = bCreateFrameBuffer ? ((VulkanRenderer*)g_Renderer)->GetFrameBufferAttachment(m_TargetColourAttachmentIDs[i]) : nullptr;

				colourAttachments[i] = vks::attachmentDescription(attachment ? attachment->format : m_ColourAttachmentFormat, m_TargetColourAttachmentFinalLayouts[i]);
				colourAttachmentReferences[i] = VkAttachmentReference{ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

				if (attachment != nullptr)
				{
					attachmentImageViews[i] = attachment->view;

					if (frameBufferWidth == -1)
					{
						frameBufferWidth = attachment->width;
						frameBufferHeight = attachment->height;
					}
				}
			}
			for (u32 i = 0; i < m_TargetColourAttachmentInitialLayouts.size(); ++i)
			{
				if (m_TargetColourAttachmentInitialLayouts[i] != VK_IMAGE_LAYOUT_UNDEFINED)
				{
					colourAttachments[i].initialLayout = m_TargetColourAttachmentInitialLayouts[i];
					colourAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				}
			}

			FrameBufferAttachment* depthAttachment = nullptr;
			if (bDepthAttachmentPresent && bCreateFrameBuffer)
			{
				depthAttachment = ((VulkanRenderer*)g_Renderer)->GetFrameBufferAttachment(m_TargetDepthAttachmentID);
				attachmentImageViews[attachmentImageViews.size() - 1] = depthAttachment->view;
				if (frameBufferWidth == -1)
				{
					frameBufferWidth = depthAttachment->width;
					frameBufferHeight = depthAttachment->height;
				}
			}

			VkAttachmentDescription depthAttachmentDesc = vks::attachmentDescription((bDepthAttachmentPresent && bCreateFrameBuffer) ? depthAttachment->format : m_DepthAttachmentFormat, m_TargetDepthAttachmentFinalLayout);

			VkAttachmentReference depthAttachmentRef = { colourAttachmentCount, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			if (m_TargetDepthAttachmentInitialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
			{
				depthAttachmentDesc.initialLayout = m_TargetDepthAttachmentInitialLayout;
				depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			}

			std::vector<VkAttachmentDescription> attachmentDescriptions(attachmentCount);
			for (u32 i = 0; i < colourAttachmentCount; ++i)
			{
				attachmentDescriptions[i] = colourAttachments[i];
			}

			if (bDepthAttachmentPresent)
			{
				attachmentDescriptions[attachmentDescriptions.size() - 1] = depthAttachmentDesc;
			}

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = (u32)colourAttachmentReferences.size();
			subpass.pColorAttachments = colourAttachmentReferences.data();
			subpass.pDepthStencilAttachment = bDepthAttachmentPresent ? &depthAttachmentRef : nullptr;

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
			renderPassCreateInfo.attachmentCount = (u32)attachmentDescriptions.size();
			renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
			renderPassCreateInfo.subpassCount = 1;
			renderPassCreateInfo.pSubpasses = &subpass;
			renderPassCreateInfo.dependencyCount = (u32)dependencies.size();
			renderPassCreateInfo.pDependencies = dependencies.data();

			if (bCreateFrameBuffer)
			{
				assert(frameBufferWidth != -1 && frameBufferHeight != -1);
			}
			Create(m_Name, &renderPassCreateInfo, attachmentImageViews, frameBufferWidth, frameBufferHeight);
		}

		void VulkanRenderPass::Register(
			const char* passName,
			const std::vector<FrameBufferAttachmentID>& targtColourAttachmentIDs,
			FrameBufferAttachmentID targtDepthAttachmentID,
			const std::vector<FrameBufferAttachmentID>& sampledAttachmentIDs)
		{
			m_TargetColourAttachmentIDs = targtColourAttachmentIDs;
			m_TargetDepthAttachmentID = targtDepthAttachmentID;
			m_SampledAttachmentIDs = sampledAttachmentIDs;
			m_Name = passName;

			m_TargetColourAttachmentFinalLayouts.resize(targtColourAttachmentIDs.size(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			m_TargetColourAttachmentInitialLayouts.resize(targtColourAttachmentIDs.size(), VK_IMAGE_LAYOUT_UNDEFINED);

			m_bRegistered = true;
		}

		void VulkanRenderPass::RegisterForColourAndDepth(
			const char* passName,
			FrameBufferAttachmentID targetColourAttachmentID,
			FrameBufferAttachmentID targetDepthAttachmentID,
			const std::vector<FrameBufferAttachmentID>& sampledAttachmentIDs)
		{
			Register(passName, { targetColourAttachmentID }, targetDepthAttachmentID, sampledAttachmentIDs);
		}

		void VulkanRenderPass::RegisterForMultiColourAndDepth(
			const char* passName,
			const std::vector<FrameBufferAttachmentID>& targetColourAttachmentIDs,
			FrameBufferAttachmentID targetDepthAttachmentID,
			const std::vector<FrameBufferAttachmentID>& sampledAttachmentIDs)
		{
			Register(passName, targetColourAttachmentIDs, targetDepthAttachmentID, sampledAttachmentIDs);
		}

		void VulkanRenderPass::RegisterForDepthOnly(
			const char* passName,
			FrameBufferAttachmentID targetDepthAttachmentID,
			const std::vector<FrameBufferAttachmentID>& sampledAttachmentIDs)
		{
			Register(passName, {}, targetDepthAttachmentID, sampledAttachmentIDs);
		}

		void VulkanRenderPass::RegisterForColourOnly(
			const char* passName,
			FrameBufferAttachmentID targetColourAttachmentID,
			const std::vector<FrameBufferAttachmentID>& sampledAttachmentIDs)
		{
			Register(passName, { targetColourAttachmentID }, InvalidFrameBufferAttachmentID, sampledAttachmentIDs);
		}

		void VulkanRenderPass::ManuallySpecifyLayouts(
			const std::vector<VkImageLayout>& finalLayouts, // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			const std::vector<VkImageLayout>& initialLayouts, // VK_IMAGE_LAYOUT_UNDEFINED
			VkImageLayout finalDepthLayout /* = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL */,
			VkImageLayout initialDepthLayout /* = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL */)
		{
			m_TargetColourAttachmentFinalLayouts = finalLayouts;
			m_TargetColourAttachmentInitialLayouts = initialLayouts;
			m_TargetDepthAttachmentFinalLayout = finalDepthLayout;
			m_TargetDepthAttachmentInitialLayout = initialDepthLayout;
		}

		VkRenderPass* VulkanRenderPass::Replace()
		{
			return m_RenderPass.replace();
		}

		VulkanRenderPass::operator VkRenderPass()
		{
			return m_RenderPass;
		}

		void VulkanRenderPass::Begin(VkCommandBuffer cmdBuf, VkClearValue* clearValues, u32 clearValueCount)
		{
			Begin(cmdBuf, clearValues, clearValueCount, m_FrameBuffer);
		}

		void VulkanRenderPass::Begin_WithFrameBuffer(VkCommandBuffer cmdBuf, VkClearValue* clearValues, u32 clearValueCount, FrameBuffer* targetFrameBuffer)
		{
			Begin(cmdBuf, clearValues, clearValueCount, targetFrameBuffer);
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

		void VulkanRenderPass::Create(
			const char* passName,
			VkRenderPassCreateInfo* inCreateInfo,
			const std::vector<VkImageView>& attachmentImageViews,
			u32 frameBufferWidth,
			u32 frameBufferHeight)
		{
			m_Name = passName;
			VK_CHECK_RESULT(vkCreateRenderPass(m_VulkanDevice->m_LogicalDevice, inCreateInfo, nullptr, m_RenderPass.replace()));
			VulkanRenderer::SetRenderPassName(m_VulkanDevice, m_RenderPass, m_Name);

			if (bCreateFrameBuffer)
			{
				VkFramebufferCreateInfo framebufferInfo = vks::framebufferCreateInfo(m_RenderPass);
				framebufferInfo.attachmentCount = (u32)attachmentImageViews.size();
				framebufferInfo.pAttachments = attachmentImageViews.data();
				framebufferInfo.width = frameBufferWidth;
				framebufferInfo.height = frameBufferHeight;
				VK_CHECK_RESULT(vkCreateFramebuffer(m_VulkanDevice->m_LogicalDevice, &framebufferInfo, nullptr, m_FrameBuffer->Replace()));

				m_FrameBuffer->width = frameBufferWidth;
				m_FrameBuffer->height = frameBufferHeight;

				char name[256];
				sprintf(name, "%s frame buffer", passName);
				VulkanRenderer::SetFramebufferName(m_VulkanDevice, m_FrameBuffer->frameBuffer, name);
			}
		}

		void VulkanRenderPass::Begin(VkCommandBuffer cmdBuf, VkClearValue* clearValues, u32 clearValueCount, FrameBuffer* targetFrameBuffer)
		{
			if (m_ActiveCommandBuffer != VK_NULL_HANDLE)
			{
				PrintError("Attempted to begin render pass (%s) multiple times! Did you forget to call End?\n", m_Name);
				return;
			}

			m_ActiveCommandBuffer = cmdBuf;

			VkRenderPassBeginInfo renderPassBeginInfo = vks::renderPassBeginInfo(m_RenderPass);

			renderPassBeginInfo.renderPass = m_RenderPass;
			renderPassBeginInfo.framebuffer = targetFrameBuffer->frameBuffer;
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = { targetFrameBuffer->width, targetFrameBuffer->height };
			renderPassBeginInfo.clearValueCount = clearValueCount;
			renderPassBeginInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}
	} // namespace vk
} // namespace flex

#endif // COMPILE_VULKAN