/**
 * @file epub_interactive.c
 * @brief Interactive EPUB Reader with keyboard navigation
 */

#include "epub_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __linux__
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#else
#include "tal_api.h"
#define usleep(x) tal_system_sleep((x) / 1000)
#endif

/* Reader state */
typedef struct {
    epub_reader_t        *reader;
    epub_config_t         config;
    int                   current_chapter;
    int                   current_offset;
    char                 *current_text;
    size_t                current_text_len;
    int                   spine_count;
    epub_content_item_t **spine_items;
    epub_toc_entry_t     *toc;
} interactive_reader_t;

/* Find next word boundary from given offset (forward) */
static int find_next_word_boundary(const char *text, int start_offset, int max_offset, int target_chars)
{
    if (!text || start_offset >= max_offset || max_offset <= 0)
        return max_offset;

    int current    = start_offset;
    int char_count = 0;
    int last_space = -1;

    /* Count characters and track the last space we encountered */
    while (current < max_offset && char_count < target_chars) {
        if (text[current] == '\0')
            break;

        /* Track spaces as potential word boundaries */
        if (isspace((unsigned char)text[current])) {
            last_space = current;
            /* If we've reached target, use this space */
            if (char_count >= target_chars - 20) {
                return current + 1; /* Return position after the space */
            }
        }

        char_count++;
        current++;
    }

    /* If we found a space near our target, use it */
    if (last_space >= start_offset && last_space < current) {
        return last_space + 1;
    }

    /* If we've reached the end, return current position */
    if (current >= max_offset) {
        return max_offset;
    }

    /* Otherwise, find the next space */
    while (current < max_offset) {
        if (text[current] == '\0')
            break;
        if (isspace((unsigned char)text[current])) {
            return current + 1;
        }
        current++;
        /* Safety limit */
        if (current - start_offset > target_chars + 200) {
            return current;
        }
    }

    return current;
}

/* Find previous word boundary from given offset (backward) */
static int find_prev_word_boundary(const char *text, int start_offset, int min_offset)
{
    if (!text || start_offset <= min_offset)
        return min_offset;

    int current = start_offset - 1;

    /* Skip trailing whitespace */
    while (current >= min_offset && isspace((unsigned char)text[current])) {
        current--;
    }

    /* Find the start of the current word */
    while (current >= min_offset && !isspace((unsigned char)text[current])) {
        current--;
    }

    /* Return position after the space (or min_offset) */
    return (current >= min_offset) ? current + 1 : min_offset;
}

/* Count words in text up to a given offset */
static int count_words_to_offset(const char *text, int offset)
{
    if (!text || offset <= 0)
        return 0;

    int word_count = 0;
    int in_word    = 0;

    for (int i = 0; i < offset && text[i] != '\0'; i++) {
        if (isspace((unsigned char)text[i])) {
            in_word = 0;
        } else {
            if (!in_word) {
                word_count++;
                in_word = 1;
            }
        }
    }

    return word_count;
}

/* Setup terminal for raw input */
static void setup_terminal(void)
{
#ifdef __linux__
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN]  = 1;
    term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
#endif
}

/* Restore terminal */
static void restore_terminal(void)
{
#ifdef __linux__
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
#endif
}

/* Get a single character from keyboard */
static int get_key(void)
{
    return getchar();
}

/* Clear screen */
static void clear_screen(void)
{
#ifdef __linux__
    printf("\033[2J\033[H");
    fflush(stdout);
#else
    /* For non-Linux, just print newlines */
    for (int i = 0; i < 50; i++) {
        printf("\n");
    }
#endif
}

/* Print a page of text */
static void print_page(interactive_reader_t *state, int offset)
{
    clear_screen();

    if (!state->current_text || offset >= (int)state->current_text_len) {
        printf("End of chapter.\n");
        return;
    }

    int chars_per_page = state->config.chars_per_page;

    /* Find word boundary for end of page */
    int end_offset;
    if (state->current_text && state->current_text_len > 0) {
        end_offset = find_next_word_boundary(state->current_text, offset, state->current_text_len, chars_per_page);
        if (end_offset > (int)state->current_text_len) {
            end_offset = state->current_text_len;
        }
        /* Ensure we have at least some content - fallback to character-based if needed */
        if (end_offset <= offset && offset < (int)state->current_text_len) {
            end_offset = offset + chars_per_page;
            if (end_offset > (int)state->current_text_len) {
                end_offset = state->current_text_len;
            }
        }
    } else {
        end_offset = offset;
    }

    /* Print header */
    printf("═══════════════════════════════════════════════════════════\n");
    if (state->spine_items && state->current_chapter < state->spine_count) {
        epub_content_item_t *item = state->spine_items[state->current_chapter];
        if (item && item->href) {
            printf("Chapter: %s\n", item->href);
        }
    }

    int current_page           = offset / chars_per_page + 1;
    int total_pages_in_chapter = (state->current_text_len + chars_per_page - 1) / chars_per_page;

    /* Safely calculate global page and progress */
    int   global_page = 1;
    float progress    = 0.0f;
    if (state->spine_items && state->current_chapter < state->spine_count) {
        epub_content_item_t *item = state->spine_items[state->current_chapter];
        if (item && item->href) {
            int total_pages = epub_get_total_pages(state->reader, &state->config);
            global_page     = epub_get_page_index(state->reader, item->href, offset, &state->config) + 1;
            progress        = epub_get_progress(state->reader, item->href, offset, &state->config);

            printf("Page: %d/%d (Chapter) | %d/%d (Total) | Progress: %.1f%%\n", current_page, total_pages_in_chapter,
                   global_page, total_pages, progress);
        } else {
            printf("Page: %d/%d (Chapter)\n", current_page, total_pages_in_chapter);
        }
    } else {
        printf("Page: %d/%d (Chapter)\n", current_page, total_pages_in_chapter);
    }

    int word_count  = count_words_to_offset(state->current_text, end_offset);
    int total_words = count_words_to_offset(state->current_text, state->current_text_len);
    printf("Words: %d/%d | Chars per page: %d | Font size: %dpt | Text length: %zu\n", word_count, total_words,
           chars_per_page, state->config.font_size, state->current_text_len);
    printf("═══════════════════════════════════════════════════════════\n\n");

    /* Print text content */
    if (state->current_text && state->current_text_len > 0) {
        /* Debug output */
        if (offset >= end_offset) {
            printf("(Offset %d >= end_offset %d, text_len: %zu)\n", offset, end_offset, state->current_text_len);
        }

        int printed_chars = 0;
        for (int i = offset; i < end_offset && i < (int)state->current_text_len; i++) {
            if (state->current_text[i] == '\0') {
                break; /* Safety check - end of string */
            } else if (state->current_text[i] == '\n') {
                printf("\n");
                printed_chars++;
            } else if (state->current_text[i] == '\r') {
                /* Skip carriage return */
            } else {
                printf("%c", state->current_text[i]);
                printed_chars++;
            }
        }

        /* If no characters were printed, show a message with debug info */
        if (printed_chars == 0) {
            if (offset < (int)state->current_text_len) {
                printf("(No text displayed: offset=%d, end_offset=%d, text_len=%zu)\n", offset, end_offset,
                       state->current_text_len);
                /* Try to show first few characters for debugging */
                if (state->current_text_len > 0) {
                    printf("First 50 chars: ");
                    int show_len = state->current_text_len > 50 ? 50 : state->current_text_len;
                    for (int i = 0; i < show_len; i++) {
                        if (state->current_text[i] >= 32 && state->current_text[i] < 127) {
                            printf("%c", state->current_text[i]);
                        } else {
                            printf("[%d]", (unsigned char)state->current_text[i]);
                        }
                    }
                    printf("\n");
                }
            }
        }
    } else {
        if (!state->current_text) {
            printf("(Text content is NULL)\n");
        } else {
            printf("(Text content length is 0)\n");
        }
    }

    printf("\n\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Commands: [N]ext page | [P]rev page | [→]Next chapter | [←]Prev chapter\n");
    printf("         [T]OC | [+]Increase font | [-]Decrease font | [H]elp | [Q]uit\n");
    printf("═══════════════════════════════════════════════════════════\n");
    fflush(stdout);
}

/* Load chapter text */
static bool load_chapter(interactive_reader_t *state, int chapter_idx)
{
    if (chapter_idx < 0 || chapter_idx >= state->spine_count) {
        return false;
    }

    epub_content_item_t *item = state->spine_items[chapter_idx];
    if (!item) {
        return false;
    }

    if (!item->href) {
        /* Chapter exists but has no href - might be a navigation item or invalid */
        return false;
    }

    /* Free previous text */
    if (state->current_text) {
        free(state->current_text);
        state->current_text = NULL;
    }

    /* Load new text */
    state->current_text = epub_get_text_content(state->reader, item->href);
    if (!state->current_text) {
        printf("Error: Failed to load text content for: %s\n", item->href ? item->href : "NULL");
        fflush(stdout);
        return false;
    }

    state->current_text_len = strlen(state->current_text);
    state->current_chapter  = chapter_idx;
    state->current_offset   = 0; /* Will be set by caller if restoring progress */

    if (state->current_text_len == 0) {
        printf("Warning: Text content is empty for: %s\n", item->href ? item->href : "NULL");
        fflush(stdout);
        /* Don't return false - allow empty chapters, but user should see the warning */
    }

    return true;
}

/* Find chapter index by href */
static int find_chapter_by_href(interactive_reader_t *state, const char *href)
{
    if (!href || !state->spine_items) {
        return -1;
    }

    /* Extract filename from href (remove fragment if present) */
    char href_copy[512];
    strncpy(href_copy, href, sizeof(href_copy) - 1);
    href_copy[sizeof(href_copy) - 1] = '\0';

    char *fragment = strchr(href_copy, '#');
    if (fragment) {
        *fragment = '\0';
    }

    /* Find matching chapter */
    for (int i = 0; i < state->spine_count; i++) {
        if (state->spine_items[i] && state->spine_items[i]->href) {
            if (strcmp(state->spine_items[i]->href, href_copy) == 0 ||
                strstr(state->spine_items[i]->href, href_copy) != NULL ||
                strstr(href_copy, state->spine_items[i]->href) != NULL) {
                return i;
            }
        }
    }

    return -1;
}

/* Show TOC */
static void show_toc(interactive_reader_t *state)
{
    clear_screen();
    printf("═══════════════════════════════════════════════════════════\n");
    printf("                    TABLE OF CONTENTS\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    if (!state->toc) {
        printf("No table of contents available.\n\n");
        printf("Press any key to return...\n");
        fflush(stdout);
        get_key();
        return;
    }

    epub_toc_entry_t *entry   = state->toc;
    int               toc_num = 1;
    printf("Select a chapter by number, or press any other key to return:\n\n");

    while (entry) {
        for (int i = 0; i < entry->level; i++) {
            printf("  ");
        }
        printf("%d. %s", toc_num++, entry->label);
        if (entry->href) {
            printf(" [%s]", entry->href);
        }
        printf("\n");
        entry = entry->next;
    }

    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("Press number key (1-9) to jump to chapter, or any other key to return: ");
    fflush(stdout);

    /* Read a single character */
    int ch = get_key();
    if (ch >= '1' && ch <= '9') {
        int selection = ch - '0';
        if (selection > 0 && selection < toc_num) {
            /* Find the selected entry */
            entry     = state->toc;
            int count = 1;
            while (entry && count < selection) {
                entry = entry->next;
                count++;
            }

            if (entry && entry->href) {
                /* Find and load the chapter */
                int chapter_idx = find_chapter_by_href(state, entry->href);
                if (chapter_idx >= 0) {
                    if (load_chapter(state, chapter_idx)) {
                        print_page(state, 0);
                        return; /* Don't show TOC again */
                    } else {
                        printf("\nFailed to load chapter. Press any key...\n");
                        fflush(stdout);
                        get_key();
                    }
                } else {
                    printf("\nChapter not found: %s. Press any key...\n", entry->href);
                    fflush(stdout);
                    get_key();
                }
            }
        }
    }
}

/* Interactive reader main loop */
void epub_interactive_reader(const char *epub_file)
{
    interactive_reader_t state = {0};

    /* Setup terminal for raw input */
    setup_terminal();

    printf("Opening EPUB file: %s\n", epub_file);
    fflush(stdout);

    /* Open EPUB */
    state.reader = epub_open(epub_file);
    if (!state.reader) {
        printf("Failed to open EPUB file: %s\n", epub_file);
        restore_terminal();
        return;
    }

    /* Get configuration */
    state.config = epub_get_default_config();
    epub_set_config(state.reader, &state.config);

    /* Get spine items */
    state.spine_items = epub_get_spine_items(state.reader, &state.spine_count);
    if (!state.spine_items || state.spine_count == 0) {
        printf("No content items found in EPUB.\n");
        epub_close(state.reader);
        restore_terminal();
        return;
    }

    /* Get TOC */
    state.toc = epub_get_toc(state.reader);

    /* Try to restore reading progress from cache */
    int saved_chapter = 0, saved_page = 0, saved_offset = 0;
    epub_get_progress_status(state.reader, &saved_chapter, &saved_page, &saved_offset);

    printf("Cache progress: Chapter=%d, Page=%d, Offset=%d\n", saved_chapter, saved_page, saved_offset);
    fflush(stdout);

    /* Load chapter from saved progress, or first valid chapter */
    int  first_chapter      = (saved_chapter >= 0 && saved_chapter < state.spine_count) ? saved_chapter : 0;
    bool loaded             = false;
    bool has_saved_progress = (saved_chapter >= 0 && saved_offset >= 0 && saved_chapter < state.spine_count);

    while (first_chapter < state.spine_count && !loaded) {
        if (load_chapter(&state, first_chapter)) {
            loaded = true;
            /* Restore saved offset if available - do this AFTER load_chapter sets it to 0 */
            if (has_saved_progress && saved_offset <= (int)state.current_text_len) {
                state.current_offset = saved_offset;
                /* Update progress in reader to match restored position */
                if (state.reader) {
                    int current_page = saved_offset / state.config.chars_per_page;
                    epub_save_progress(state.reader, first_chapter, current_page, saved_offset);
                }
                printf("Restored reading position: Chapter %d, Offset %d characters\n", first_chapter + 1,
                       saved_offset);
                fflush(stdout);
            } else {
                state.current_offset = 0;
                if (has_saved_progress) {
                    printf("Warning: Saved offset %d exceeds chapter length %zu, starting at beginning\n", saved_offset,
                           state.current_text_len);
                    fflush(stdout);
                }
            }
        } else {
            printf("Skipping invalid chapter %d at start...\n", first_chapter + 1);
            fflush(stdout);
            first_chapter++;
        }
    }

    if (!loaded) {
        printf("No valid chapters found in EPUB.\n");
        epub_close(state.reader);
        restore_terminal();
        return;
    }

    /* Display page at saved position or first page */
    print_page(&state, state.current_offset);

    /* Main input loop */
    int ch;
    while (1) {
        ch = get_key();

        if (ch == EOF) {
            continue;
        }

        ch = tolower(ch);

        switch (ch) {
        case 'n': /* Next page */
        {
            int chars_per_page = state.config.chars_per_page;
            /* Find next word boundary instead of just adding chars */
            int new_offset = find_next_word_boundary(state.current_text, state.current_offset, state.current_text_len,
                                                     chars_per_page);
            if (new_offset >= (int)state.current_text_len) {
                /* Try next chapter - skip invalid ones */
                int  next_chapter = state.current_chapter + 1;
                bool loaded       = false;
                while (next_chapter < state.spine_count && !loaded) {
                    if (load_chapter(&state, next_chapter)) {
                        print_page(&state, 0);
                        loaded = true;
                    } else {
                        printf("Skipping invalid chapter %d...\n", next_chapter + 1);
                        fflush(stdout);
                        next_chapter++;
                    }
                }
                if (!loaded) {
                    printf("\nEnd of book! Press any key to continue...\n");
                    fflush(stdout);
                    get_key();
                    print_page(&state, state.current_offset);
                }
            } else {
                state.current_offset = new_offset;
                print_page(&state, state.current_offset);
            }
            /* Save progress after navigation */
            if (state.reader) {
                int current_page = state.current_offset / chars_per_page;
                epub_save_progress(state.reader, state.current_chapter, current_page, state.current_offset);
            }
        } break;

        case 'p': /* Previous page */
        {
            int chars_per_page = state.config.chars_per_page;
            /* Find previous word boundary */
            int target_offset = state.current_offset - chars_per_page;
            if (target_offset < 0) {
                target_offset = 0;
            }
            int new_offset = find_prev_word_boundary(state.current_text, target_offset, 0);
            if (new_offset < 0 || new_offset >= state.current_offset) {
                /* If we can't go back enough, try going back one word boundary from current */
                new_offset = find_prev_word_boundary(state.current_text, state.current_offset, 0);
            }
            if (new_offset < 0) {
                /* Try previous chapter - skip invalid ones */
                int  prev_chapter = state.current_chapter - 1;
                bool loaded       = false;
                while (prev_chapter >= 0 && !loaded) {
                    if (load_chapter(&state, prev_chapter)) {
                        /* Find a good starting position near the end using word boundaries */
                        int target = state.current_text_len - chars_per_page;
                        if (target < 0)
                            target = 0;
                        int last_page_offset = find_prev_word_boundary(state.current_text, target, 0);
                        print_page(&state, last_page_offset);
                        state.current_offset = last_page_offset;
                        loaded               = true;
                    } else {
                        printf("Skipping invalid chapter %d...\n", prev_chapter + 1);
                        fflush(stdout);
                        prev_chapter--;
                    }
                }
                if (!loaded) {
                    printf("\nBeginning of book! Press any key to continue...\n");
                    fflush(stdout);
                    get_key();
                    print_page(&state, state.current_offset);
                }
            } else {
                state.current_offset = new_offset;
                print_page(&state, state.current_offset);
            }
            /* Save progress after navigation */
            if (state.reader) {
                int current_page = state.current_offset / chars_per_page;
                epub_save_progress(state.reader, state.current_chapter, current_page, state.current_offset);
            }
        } break;

        case 27: /* ESC or arrow key prefix */
        {
            /* Check for arrow keys - read next character */
            int ch2 = get_key();
            if (ch2 == '[') {
                ch2 = get_key();
                if (ch2 == 'C') { /* Right arrow - Next chapter */
                    int  next_chapter = state.current_chapter + 1;
                    bool loaded       = false;
                    while (next_chapter < state.spine_count && !loaded) {
                        if (load_chapter(&state, next_chapter)) {
                            print_page(&state, 0);
                            loaded = true;
                        } else {
                            printf("Skipping invalid chapter %d...\n", next_chapter + 1);
                            fflush(stdout);
                            next_chapter++;
                        }
                    }
                    if (!loaded) {
                        printf("\nLast chapter! Press any key to continue...\n");
                        fflush(stdout);
                        get_key();
                        print_page(&state, state.current_offset);
                    }
                    /* Save progress */
                    if (state.reader && loaded) {
                        epub_save_progress(state.reader, state.current_chapter, 0, 0);
                    }
                } else if (ch2 == 'D') { /* Left arrow - Previous chapter */
                    int  prev_chapter = state.current_chapter - 1;
                    bool loaded       = false;
                    while (prev_chapter >= 0 && !loaded) {
                        if (load_chapter(&state, prev_chapter)) {
                            print_page(&state, 0);
                            loaded = true;
                        } else {
                            printf("Skipping invalid chapter %d...\n", prev_chapter + 1);
                            fflush(stdout);
                            prev_chapter--;
                        }
                    }
                    if (!loaded) {
                        printf("\nFirst chapter! Press any key to continue...\n");
                        fflush(stdout);
                        get_key();
                        print_page(&state, state.current_offset);
                    }
                    /* Save progress */
                    if (state.reader && loaded) {
                        epub_save_progress(state.reader, state.current_chapter, 0, 0);
                    }
                }
            } else if (ch2 == EOF || ch2 == 27) {
                /* Double ESC or ESC alone - ignore */
            }
        } break;

        case 't': /* Show TOC */
            show_toc(&state);
            print_page(&state, state.current_offset);
            break;

        case '+':
        case '=': /* Increase font (decrease chars per page) */
            if (state.config.chars_per_page > 500) {
                state.config.chars_per_page -= 200;
                epub_set_config(state.reader, &state.config);
                printf("\nFont size increased. Chars per page: %d\n", state.config.chars_per_page);
                fflush(stdout);
                usleep(500000);
                print_page(&state, state.current_offset);
            } else {
                printf("\nMaximum font size reached! Press any key...\n");
                fflush(stdout);
                get_key();
                print_page(&state, state.current_offset);
            }
            break;

        case '-':
        case '_': /* Decrease font (increase chars per page) */
            if (state.config.chars_per_page < 5000) {
                state.config.chars_per_page += 200;
                epub_set_config(state.reader, &state.config);
                printf("\nFont size decreased. Chars per page: %d\n", state.config.chars_per_page);
                fflush(stdout);
                usleep(500000);
                print_page(&state, state.current_offset);
            } else {
                printf("\nMinimum font size reached! Press any key...\n");
                fflush(stdout);
                get_key();
                print_page(&state, state.current_offset);
            }
            break;

        case 'q': /* Quit */
            goto cleanup;

        case 'h': /* Help */
        {
            clear_screen();
            printf("═══════════════════════════════════════════════════════════\n");
            printf("                         HELP\n");
            printf("═══════════════════════════════════════════════════════════\n\n");
            printf("Keyboard Commands:\n\n");
            printf("  [N] or [n]     - Next page\n");
            printf("  [P] or [p]     - Previous page\n");
            printf("  [→] (Right)    - Next chapter\n");
            printf("  [←] (Left)     - Previous chapter\n");
            printf("  [T] or [t]     - Show Table of Contents\n");
            printf("  [+] or [=]     - Increase font size (less text per page)\n");
            printf("  [-] or [_]     - Decrease font size (more text per page)\n");
            printf("  [H] or [h]     - Show this help\n");
            printf("  [Q] or [q]     - Quit\n\n");
            printf("═══════════════════════════════════════════════════════════\n");
            printf("Press any key to return...\n");
            fflush(stdout);
            get_key();
            print_page(&state, state.current_offset);
        } break;

        default:
            /* Ignore other keys */
            break;
        }
    }

cleanup:
    /* Cleanup */
    if (state.current_text) {
        free(state.current_text);
    }
    epub_close(state.reader);
    restore_terminal();
    clear_screen();
    printf("Thank you for reading!\n");
    fflush(stdout);
}
