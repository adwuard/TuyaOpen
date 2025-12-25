/**
 * @file epub_reader.h
 * @brief EPUB3 Reader API for parsing and reading EPUB files
 *
 * This module provides APIs for:
 * - Opening and parsing EPUB3 files
 * - Reading Table of Contents (TOC)
 * - Parsing XML content (OPF, NCX, XHTML)
 * - Calculating text size and pages
 * - Progress tracking
 */

#ifndef EPUB_READER_H
#define EPUB_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct epub_reader_s    epub_reader_t;
typedef struct epub_toc_entry_s epub_toc_entry_t;
typedef struct epub_metadata_s  epub_metadata_t;

/**
 * @brief EPUB metadata structure
 */
struct epub_metadata_s {
    char *title;
    char *author;
    char *language;
    char *publisher;
    char *date;
    char *identifier;
};

/**
 * @brief Table of Contents entry structure
 */
struct epub_toc_entry_s {
    char             *label;      /* Display label */
    char             *href;       /* Reference to content file */
    int               level;      /* Hierarchy level (1, 2, 3, ...) */
    int               page_index; /* Calculated page index */
    epub_toc_entry_t *next;       /* Next entry at same level */
    epub_toc_entry_t *children;   /* Child entries */
};

/**
 * @brief EPUB reader configuration
 */
typedef struct {
    int   chars_per_page; /* Characters per page for pagination */
    int   font_size;      /* Font size in points (for text size calculation) */
    int   line_height;    /* Line height multiplier */
    int   page_width;     /* Page width in pixels */
    int   page_height;    /* Page height in pixels */
    char *font_path;      /* Font path/name (e.g., "lv_font_montserrat_14" for LVGL) */
} epub_config_t;

/**
 * @brief EPUB content item
 */
typedef struct {
    char  *id;           /* Item ID */
    char  *href;         /* File path */
    char  *media_type;   /* MIME type */
    char  *content;      /* Parsed text content */
    size_t content_size; /* Content size in bytes */
    int    page_count;   /* Number of pages in this item */
} epub_content_item_t;

/**
 * @brief Open an EPUB3 file
 *
 * @param filepath Path to the EPUB file
 * @return epub_reader_t* Reader handle, NULL on error
 */
epub_reader_t *epub_open(const char *filepath);

/**
 * @brief Close EPUB reader and free resources
 *
 * @param reader Reader handle
 */
void epub_close(epub_reader_t *reader);

/**
 * @brief Get EPUB metadata
 *
 * @param reader Reader handle
 * @return epub_metadata_t* Metadata structure, NULL on error
 */
epub_metadata_t *epub_get_metadata(epub_reader_t *reader);

/**
 * @brief Free metadata structure
 *
 * @param metadata Metadata to free
 */
void epub_free_metadata(epub_metadata_t *metadata);

/**
 * @brief Get Table of Contents
 *
 * @param reader Reader handle
 * @return epub_toc_entry_t* Root TOC entry, NULL on error
 */
epub_toc_entry_t *epub_get_toc(epub_reader_t *reader);

/**
 * @brief Free TOC structure
 *
 * @param toc Root TOC entry
 */
void epub_free_toc(epub_toc_entry_t *toc);

/**
 * @brief Get content item by ID
 *
 * @param reader Reader handle
 * @param item_id Item ID
 * @return epub_content_item_t* Content item, NULL if not found
 */
epub_content_item_t *epub_get_content_item(epub_reader_t *reader, const char *item_id);

/**
 * @brief Get content item by href
 *
 * @param reader Reader handle
 * @param href File path/href
 * @return epub_content_item_t* Content item, NULL if not found
 */
epub_content_item_t *epub_get_content_by_href(epub_reader_t *reader, const char *href);

/**
 * @brief Free content item
 *
 * @param item Content item to free
 */
void epub_free_content_item(epub_content_item_t *item);

/**
 * @brief Get text content from XHTML (stripped of HTML tags)
 *
 * @param reader Reader handle
 * @param href Content file href
 * @return char* Plain text content, NULL on error (caller must free)
 */
char *epub_get_text_content(epub_reader_t *reader, const char *href);

/**
 * @brief Calculate text size for a given string
 *
 * @param text Text to measure
 * @param config Reader configuration
 * @return size_t Text size in bytes
 */
size_t epub_calculate_text_size(const char *text, const epub_config_t *config);

/**
 * @brief Calculate number of pages for text content
 *
 * @param text Text content
 * @param config Reader configuration
 * @return int Number of pages
 */
int epub_calculate_pages(const char *text, const epub_config_t *config);

/**
 * @brief Get total page count for entire EPUB
 *
 * @param reader Reader handle
 * @param config Reader configuration
 * @return int Total number of pages
 */
int epub_get_total_pages(epub_reader_t *reader, const epub_config_t *config);

/**
 * @brief Get current page index for a content position
 *
 * @param reader Reader handle
 * @param href Content file href
 * @param char_offset Character offset in content
 * @param config Reader configuration
 * @return int Page index (0-based)
 */
int epub_get_page_index(epub_reader_t *reader, const char *href, int char_offset, const epub_config_t *config);

/**
 * @brief Get progress percentage (0-100)
 *
 * @param reader Reader handle
 * @param href Current content file href
 * @param char_offset Current character offset
 * @param config Reader configuration
 * @return float Progress percentage (0.0 to 100.0)
 */
float epub_get_progress(epub_reader_t *reader, const char *href, int char_offset, const epub_config_t *config);

/**
 * @brief Set reader configuration
 *
 * @param reader Reader handle
 * @param config Configuration structure
 */
void epub_set_config(epub_reader_t *reader, const epub_config_t *config);

/**
 * @brief Get default configuration
 *
 * @return epub_config_t Default configuration
 */
epub_config_t epub_get_default_config(void);

/**
 * @brief Parse XML string and extract text content
 *
 * @param xml XML string
 * @param tag_name Tag name to extract (NULL for all text)
 * @return char* Extracted text (caller must free), NULL on error
 */
char *epub_parse_xml_text(const char *xml, const char *tag_name);

/**
 * @brief Extract attribute value from XML tag
 *
 * @param xml XML string
 * @param tag_name Tag name
 * @param attr_name Attribute name
 * @return char* Attribute value (caller must free), NULL if not found
 */
char *epub_parse_xml_attr(const char *xml, const char *tag_name, const char *attr_name);

/**
 * @brief Get list of all content items (spine order)
 *
 * @param reader Reader handle
 * @param count Output parameter for number of items
 * @return epub_content_item_t** Array of content items (caller must free array and items)
 */
epub_content_item_t **epub_get_spine_items(epub_reader_t *reader, int *count);

/**
 * @brief Start interactive EPUB reader with keyboard navigation
 *
 * @param epub_file Path to EPUB file
 */
void epub_interactive_reader(const char *epub_file);

/**
 * @brief Save reading progress to cache
 *
 * @param reader Reader handle
 * @param chapter Current chapter index in spine
 * @param page Current page within chapter
 * @param char_offset Current character offset within chapter
 */
void epub_save_progress(epub_reader_t *reader, int chapter, int page, int char_offset);

/**
 * @brief Get reading progress status
 *
 * @param reader Reader handle
 * @param chapter Output parameter for current chapter (can be NULL)
 * @param page Output parameter for current page (can be NULL)
 * @param char_offset Output parameter for current character offset (can be NULL)
 */
void epub_get_progress_status(epub_reader_t *reader, int *chapter, int *page, int *char_offset);

#ifdef __cplusplus
}
#endif

#endif /* EPUB_READER_H */
