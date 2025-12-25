#include "tal_api.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "epub_reader.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief Demo function to test EPUB reader functionality
 */
static void epub_reader_demo(const char *epub_file)
{
    PR_DEBUG("EPUB Reader Demo - Opening file: %s\r\n", epub_file);

    /* Open EPUB file */
    epub_reader_t *reader = epub_open(epub_file);
    if (!reader) {
        PR_DEBUG("Failed to open EPUB file: %s\r\n", epub_file);
        return;
    }

    PR_DEBUG("EPUB file opened successfully\r\n");

    /* Get and display metadata */
    epub_metadata_t *metadata = epub_get_metadata(reader);
    if (metadata) {
        PR_DEBUG("=== EPUB Metadata ===\r\n");
        if (metadata->title) {
            PR_DEBUG("Title: %s\r\n", metadata->title);
        }
        if (metadata->author) {
            PR_DEBUG("Author: %s\r\n", metadata->author);
        }
        if (metadata->language) {
            PR_DEBUG("Language: %s\r\n", metadata->language);
        }
        if (metadata->publisher) {
            PR_DEBUG("Publisher: %s\r\n", metadata->publisher);
        }
        if (metadata->date) {
            PR_DEBUG("Date: %s\r\n", metadata->date);
        }
        PR_DEBUG("\r\n");
    }

    /* Get and display TOC */
    epub_toc_entry_t *toc = epub_get_toc(reader);
    if (toc) {
        PR_DEBUG("=== Table of Contents ===\r\n");
        epub_toc_entry_t *entry     = toc;
        int               toc_count = 0;
        while (entry) {
            for (int i = 0; i < entry->level; i++) {
                PR_DEBUG("  ");
            }
            PR_DEBUG("%d. %s -> %s\r\n", ++toc_count, entry->label, entry->href);
            entry = entry->next;
        }
        PR_DEBUG("\r\n");
    } else {
        PR_DEBUG("No TOC found\r\n\r\n");
    }

    /* Get configuration */
    epub_config_t config = epub_get_default_config();
    PR_DEBUG("=== Reader Configuration ===\r\n");
    PR_DEBUG("Chars per page: %d\r\n", config.chars_per_page);
    PR_DEBUG("Font size: %dpt\r\n", config.font_size);
    PR_DEBUG("Page size: %dx%d\r\n", config.page_width, config.page_height);
    PR_DEBUG("\r\n");

    /* Get spine items and calculate pages */
    int                   spine_count = 0;
    epub_content_item_t **spine_items = epub_get_spine_items(reader, &spine_count);
    if (spine_items && spine_count > 0) {
        PR_DEBUG("=== Content Items (Spine Order) ===\r\n");
        PR_DEBUG("Total items: %d\r\n\r\n", spine_count);

        int total_pages = epub_get_total_pages(reader, &config);
        PR_DEBUG("Total pages: %d\r\n\r\n", total_pages);

        /* Display first few items */
        int display_count = spine_count > 5 ? 5 : spine_count;
        for (int i = 0; i < display_count; i++) {
            if (spine_items[i] && spine_items[i]->href) {
                PR_DEBUG("Item %d: %s\r\n", i + 1, spine_items[i]->href);

                /* Get text content */
                char *text = epub_get_text_content(reader, spine_items[i]->href);
                if (text) {
                    size_t text_size = epub_calculate_text_size(text, &config);
                    int    pages     = epub_calculate_pages(text, &config);

                    PR_DEBUG("  Text size: %zu bytes\r\n", text_size);
                    PR_DEBUG("  Pages: %d\r\n", pages);

                    /* Show first 200 characters */
                    int  preview_len = text_size > 200 ? 200 : text_size;
                    char preview[201];
                    strncpy(preview, text, preview_len);
                    preview[preview_len] = '\0';
                    PR_DEBUG("  Preview: %s...\r\n", preview);

                    /* Calculate progress for this item */
                    float progress = epub_get_progress(reader, spine_items[i]->href, 0, &config);
                    PR_DEBUG("  Progress at start: %.2f%%\r\n", progress);

                    free(text);
                }
                PR_DEBUG("\r\n");
            }
        }

        if (spine_count > display_count) {
            PR_DEBUG("... and %d more items\r\n\r\n", spine_count - display_count);
        }
    }

    /* Demo progress calculation */
    if (spine_items && spine_count > 0 && spine_items[0] && spine_items[0]->href) {
        PR_DEBUG("=== Progress Calculation Demo ===\r\n");
        char *text = epub_get_text_content(reader, spine_items[0]->href);
        if (text) {
            size_t text_len = strlen(text);
            PR_DEBUG("First item text length: %zu characters\r\n", text_len);

            /* Calculate progress at different positions */
            int positions[] = {0, text_len / 4, text_len / 2, text_len * 3 / 4, text_len};
            for (int i = 0; i < 5; i++) {
                int pos = positions[i];
                if (pos > text_len)
                    pos = text_len;

                int   page_idx = epub_get_page_index(reader, spine_items[0]->href, pos, &config);
                float progress = epub_get_progress(reader, spine_items[0]->href, pos, &config);

                PR_DEBUG("Position %d: Page %d, Progress: %.2f%%\r\n", pos, page_idx, progress);
            }

            free(text);
        }
        PR_DEBUG("\r\n");
    }

    /* Close reader */
    epub_close(reader);
    PR_DEBUG("EPUB reader closed\r\n");
}

static void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    PR_DEBUG("EPUB Reader Application\r\n");
    PR_DEBUG("======================\r\n\r\n");

#if OPERATING_SYSTEM == SYSTEM_LINUX
    /* On Linux, check for command line argument */
    const char *epub_file = getenv("EPUB_FILE");
    if (epub_file && strlen(epub_file) > 0) {
        /* Start interactive reader */
        epub_interactive_reader(epub_file);
    } else {
        PR_DEBUG("Usage: Set EPUB_FILE environment variable or pass file as argument\r\n");
        PR_DEBUG("Example: EPUB_FILE=/path/to/book.epub %s\r\n", "epub_reader");
        PR_DEBUG("        %s /path/to/book.epub\r\n", "epub_reader");
        PR_DEBUG("\r\n");
        PR_DEBUG("No EPUB file provided.\r\n");
        PR_DEBUG("Please set EPUB_FILE environment variable or pass file path as argument.\r\n");
    }
#else
    /* For embedded systems, you can hardcode a path or use file system */
    PR_DEBUG("Running on embedded system. EPUB file path should be configured.\r\n");
    /* epub_interactive_reader("/path/to/book.epub"); */
#endif

    /* Exit after interactive reader closes */
    PR_DEBUG("Application exiting.\r\n");
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    /* Store command line arguments for use in user_main */
    if (argc > 1) {
        setenv("EPUB_FILE", argv[1], 1);
    }
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif