/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkqueue.h"

/**
 * SECTION:vkqueue
 * @title: GstVulkanQueue
 * @short_description: Vulkan command queue
 * @see_also: #GstVulkanDevice
 *
 * GstVulkanQueue encapsulates the vulkan command queue.
 */

#define GST_CAT_DEFAULT gst_vulkan_queue_debug
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);
GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);

#define parent_class gst_vulkan_queue_parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanQueue, gst_vulkan_queue, GST_TYPE_OBJECT,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "vulkanqueue", 0,
        "Vulkan Queue");
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT"));

static void gst_vulkan_queue_dispose (GObject * object);

static void
gst_vulkan_queue_init (GstVulkanQueue * device)
{
}

static void
gst_vulkan_queue_class_init (GstVulkanQueueClass * device_class)
{
  GObjectClass *gobject_class = (GObjectClass *) device_class;

  gobject_class->dispose = gst_vulkan_queue_dispose;
}

static void
gst_vulkan_queue_dispose (GObject * object)
{
  GstVulkanQueue *queue = GST_VULKAN_QUEUE (object);

  if (queue->device)
    gst_object_unref (queue->device);
  queue->device = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/**
 * gst_vulkan_queue_get_device
 * @queue: a #GstVulkanQueue
 *
 * Returns: (transfer full): the #GstVulkanDevice for @queue
 *
 * Since: 1.18
 */
GstVulkanDevice *
gst_vulkan_queue_get_device (GstVulkanQueue * queue)
{
  g_return_val_if_fail (GST_IS_VULKAN_QUEUE (queue), NULL);

  return queue->device ? gst_object_ref (queue->device) : NULL;
}

/**
 * gst_vulkan_queue_create_command_pool:
 * @queue: a #GstVulkanQueue
 * @error: a #GError
 *
 * Returns: (transfer full): a new #GstVUlkanCommandPool or %NULL
 *
 * Since: 1.18
 */
GstVulkanCommandPool *
gst_vulkan_queue_create_command_pool (GstVulkanQueue * queue, GError ** error)
{
  GstVulkanCommandPool *pool;
  VkCommandPoolCreateInfo cmd_pool_info = { 0, };
  VkCommandPool vk_pool;
  VkResult err;

  g_return_val_if_fail (GST_IS_VULKAN_QUEUE (queue), NULL);

  cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmd_pool_info.pNext = NULL;
  cmd_pool_info.queueFamilyIndex = queue->family;
  cmd_pool_info.flags = 0;

  GST_OBJECT_LOCK (queue->device);
  err =
      vkCreateCommandPool (queue->device->device, &cmd_pool_info, NULL,
      &vk_pool);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateCommandPool") < 0) {
    GST_OBJECT_LOCK (queue->device);
    goto error;
  }
  GST_OBJECT_UNLOCK (queue->device);

  pool = g_object_new (GST_TYPE_VULKAN_COMMAND_POOL, NULL);
  gst_object_ref_sink (pool);
  pool->queue = gst_object_ref (queue);
  pool->pool = vk_pool;

  return pool;

error:
  return NULL;
}

/**
 * gst_context_set_vulkan_queue:
 * @context: a #GstContext
 * @queue: a #GstVulkanQueue
 *
 * Sets @queue on @context
 *
 * Since: 1.18
 */
void
gst_context_set_vulkan_queue (GstContext * context, GstVulkanQueue * queue)
{
  GstStructure *s;

  g_return_if_fail (context != NULL);
  g_return_if_fail (gst_context_is_writable (context));

  if (queue)
    GST_CAT_LOG (GST_CAT_CONTEXT,
        "setting GstVulkanQueue(%" GST_PTR_FORMAT ") on context(%"
        GST_PTR_FORMAT ")", queue, context);

  s = gst_context_writable_structure (context);
  gst_structure_set (s, GST_VULKAN_QUEUE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_QUEUE, queue, NULL);
}

/**
 * gst_context_get_vulkan_queue:
 * @context: a #GstContext
 * @queue: resulting #GstVulkanQueue
 *
 * Returns: Whether @queue was in @context
 *
 * Since: 1.18
 */
gboolean
gst_context_get_vulkan_queue (GstContext * context, GstVulkanQueue ** queue)
{
  const GstStructure *s;
  gboolean ret;

  g_return_val_if_fail (queue != NULL, FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  s = gst_context_get_structure (context);
  ret = gst_structure_get (s, GST_VULKAN_QUEUE_CONTEXT_TYPE_STR,
      GST_TYPE_VULKAN_QUEUE, queue, NULL);

  GST_CAT_LOG (GST_CAT_CONTEXT, "got GstVulkanQueue(%" GST_PTR_FORMAT
      ") from context(%" GST_PTR_FORMAT ")", *queue, context);

  return ret;
}

/**
 * gst_vulkan_queue_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery of type #GST_QUERY_CONTEXT
 * @queue: the #GstVulkanQueue
 *
 * If a #GstVulkanQueue is requested in @query, sets @queue as the reply.
 *
 * Intended for use with element query handlers to respond to #GST_QUERY_CONTEXT
 * for a #GstVulkanQueue.
 *
 * Returns: whether @query was responded to with @queue
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_queue_handle_context_query (GstElement * element, GstQuery * query,
    GstVulkanQueue ** queue)
{
  gboolean res = FALSE;
  const gchar *context_type;
  GstContext *context, *old_context;

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (query != NULL, FALSE);
  g_return_val_if_fail (GST_QUERY_TYPE (query) == GST_QUERY_CONTEXT, FALSE);
  g_return_val_if_fail (queue != NULL, FALSE);

  gst_query_parse_context_type (query, &context_type);

  if (g_strcmp0 (context_type, GST_VULKAN_QUEUE_CONTEXT_TYPE_STR) == 0) {
    gst_query_parse_context (query, &old_context);

    if (old_context)
      context = gst_context_copy (old_context);
    else
      context = gst_context_new (GST_VULKAN_QUEUE_CONTEXT_TYPE_STR, TRUE);

    gst_context_set_vulkan_queue (context, *queue);
    gst_query_set_context (query, context);
    gst_context_unref (context);

    res = *queue != NULL;
  }

  return res;
}

/**
 * gst_vulkan_queue_run_context_query:
 * @element: a #GstElement
 * @queue: (inout): a #GstVulkanQueue
 *
 * Attempt to retrieve a #GstVulkanQueue using #GST_QUERY_CONTEXT from the
 * surrounding elements of @element.
 *
 * Returns: whether @queue contains a valid #GstVulkanQueue
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_queue_run_context_query (GstElement * element,
    GstVulkanQueue ** queue)
{
  GstQuery *query;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);
  g_return_val_if_fail (queue != NULL, FALSE);

  if (*queue && GST_IS_VULKAN_QUEUE (*queue))
    return TRUE;

  if ((query =
          gst_vulkan_local_context_query (element,
              GST_VULKAN_QUEUE_CONTEXT_TYPE_STR))) {
    GstContext *context;

    gst_query_parse_context (query, &context);
    if (context)
      gst_context_get_vulkan_queue (context, queue);

    gst_query_unref (query);
  }

  GST_DEBUG_OBJECT (element, "found queue %p", *queue);

  if (*queue)
    return TRUE;

  return FALSE;
}
