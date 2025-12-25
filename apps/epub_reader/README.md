# EPUB3 Reader

A simple EPUB3 reader implementation for parsing and reading EPUB files with e-reader features.

## Features

- **EPUB3 File Parsing**: Opens and parses EPUB3 files (ZIP archives)
- **XML Parsing**: Parses OPF, NCX, and XHTML content files
- **Table of Contents (TOC)**: Reads and navigates TOC from nav.xhtml or toc.ncx
- **Text Size Calculation**: Calculates text size for content
- **Pagination**: Calculates text per page and total pages
- **Progress Tracking**: Tracks reading progress with page indexing

## API Overview

### Opening and Closing EPUB Files

```c
epub_reader_t *epub_open(const char *filepath);
void epub_close(epub_reader_t *reader);
```

### Metadata

```c
epub_metadata_t *epub_get_metadata(epub_reader_t *reader);
void epub_free_metadata(epub_metadata_t *metadata);
```

### Table of Contents

```c
epub_toc_entry_t *epub_get_toc(epub_reader_t *reader);
void epub_free_toc(epub_toc_entry_t *toc);
```

### Content Reading

```c
char *epub_get_text_content(epub_reader_t *reader, const char *href);
epub_content_item_t *epub_get_content_by_href(epub_reader_t *reader, const char *href);
```

### Pagination and Progress

```c
int epub_calculate_pages(const char *text, const epub_config_t *config);
int epub_get_total_pages(epub_reader_t *reader, const epub_config_t *config);
int epub_get_page_index(epub_reader_t *reader, const char *href, int char_offset, const epub_config_t *config);
float epub_get_progress(epub_reader_t *reader, const char *href, int char_offset, const epub_config_t *config);
```

### Configuration

```c
epub_config_t epub_get_default_config(void);
void epub_set_config(epub_reader_t *reader, const epub_config_t *config);
```

## Usage Example

```c
#include "epub_reader.h"

/* Open EPUB file */
epub_reader_t *reader = epub_open("/path/to/book.epub");
if (!reader) {
    printf("Failed to open EPUB file\n");
    return;
}

/* Get metadata */
epub_metadata_t *metadata = epub_get_metadata(reader);
if (metadata && metadata->title) {
    printf("Title: %s\n", metadata->title);
}

/* Get TOC */
epub_toc_entry_t *toc = epub_get_toc(reader);
epub_toc_entry_t *entry = toc;
while (entry) {
    printf("TOC: %s -> %s\n", entry->label, entry->href);
    entry = entry->next;
}

/* Get configuration */
epub_config_t config = epub_get_default_config();
config.chars_per_page = 2000;  /* Adjust as needed */

/* Calculate total pages */
int total_pages = epub_get_total_pages(reader, &config);
printf("Total pages: %d\n", total_pages);

/* Get text content */
char *text = epub_get_text_content(reader, "chapter1.xhtml");
if (text) {
    int pages = epub_calculate_pages(text, &config);
    printf("Chapter pages: %d\n", pages);
    free(text);
}

/* Calculate progress */
float progress = epub_get_progress(reader, "chapter1.xhtml", 1000, &config);
printf("Progress: %.2f%%\n", progress);

/* Clean up */
epub_free_toc(toc);
epub_free_metadata(metadata);
epub_close(reader);
```

## Building on Linux

The implementation uses the `unzip` command for extracting EPUB files on Linux. Make sure `unzip` is installed:

```bash
sudo apt-get install unzip
```

Alternatively, you can use libzip library for better performance:

```bash
sudo apt-get install libzip-dev
```

Then define `HAVE_LIBZIP` in your build configuration.

## Building

The project uses CMake. Build it as part of the TuyaOpen project:

```bash
cd /path/to/TuyaOpen
# Build the project (epub_reader will be included)
```

## Running the Demo

On Linux, you can run the demo application:

```bash
# Set EPUB file path as environment variable
EPUB_FILE=/path/to/book.epub ./epub_reader

# Or pass as command line argument
./epub_reader /path/to/book.epub
```

## EPUB File Structure

EPUB3 files are ZIP archives containing:

- `META-INF/container.xml` - Points to the OPF file
- `OEBPS/content.opf` - Package file with metadata and manifest
- `OEBPS/toc.xhtml` or `toc.ncx` - Table of contents
- `OEBPS/*.xhtml` - Content files (chapters, etc.)

## Configuration

The default configuration provides:
- 2000 characters per page
- 12pt font size
- 600x800 pixel page size

You can customize these values:

```c
epub_config_t config = epub_get_default_config();
config.chars_per_page = 1500;  /* Fewer chars = more pages */
config.font_size = 14;          /* Larger font */
epub_set_config(reader, &config);
```

## Limitations

- Basic XML parsing (not full XML parser)
- Simple HTML tag stripping
- Requires `unzip` command or libzip library on Linux
- TOC parsing is simplified (may not handle all EPUB3 nav structures)

## Future Enhancements

- Full XML parser support
- Better HTML/CSS parsing
- Image support
- Bookmark support
- Reading position persistence
- Font rendering and text layout

