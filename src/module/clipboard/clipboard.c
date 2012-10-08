/***************************************************************************
 *   Copyright (C) 2012~2012 by Yichao Yu                                  *
 *   yyc1992@gmail.com                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include "fcitx/fcitx.h"
#include "config.h"
#include "fcitx/ime.h"
#include "fcitx/instance.h"
#include "fcitx/context.h"
#include "fcitx/module.h"
#include "fcitx/frontend.h"
#include "fcitx/candidate.h"
#include "fcitx/hook.h"
#include "fcitx/keys.h"
#include "fcitx-config/xdg.h"
#include "fcitx-utils/log.h"
#include "fcitx-utils/utf8.h"

#include "clipboard-internal.h"
#ifdef ENABLE_X11
#include "clipboard-x11.h"
#endif

CONFIG_DEFINE_LOAD_AND_SAVE(FcitxClipboard, FcitxClipboardConfig,
                            "fcitx-clipboard");

static void *ClipboardCreate(FcitxInstance *instance);
static void ClipboardDestroy(void *arg);
static void ClipboardReloadConfig(void *arg);
static void ApplyClipboardConfig(FcitxClipboard *clipboard);

FCITX_DEFINE_PLUGIN(fcitx_clipboard, module, FcitxModule) = {
    .Create = ClipboardCreate,
    .Destroy = ClipboardDestroy,
    .SetFD = NULL,
    .ProcessEvent = NULL,
    .ReloadConfig = ClipboardReloadConfig
};

static void
ClipboardWriteHistory(FcitxClipboard *clipboard)
{
    FILE *fp;
    fp = FcitxXDGGetFileUserWithPrefix("clipboard", "history.dat", "w", NULL);
    if (!fp)
        return;
    fcitx_utils_write_uint32(fp, clipboard->clp_hist_len);
    fcitx_utils_write_uint32(fp, clipboard->primary.len);
    int i;
    for (i = 0;i < clipboard->clp_hist_len;i++) {
        fcitx_utils_write_uint32(fp, clipboard->clp_hist_lst[i].len);
    }
    if (clipboard->primary.str)
        fwrite(clipboard->primary.str, 1, clipboard->primary.len, fp);
    for (i = 0;i < clipboard->clp_hist_len;i++) {
        if (clipboard->clp_hist_lst[i].str)
            fwrite(clipboard->clp_hist_lst[i].str, 1,
                   clipboard->clp_hist_lst[i].len, fp);
    }
    fclose(fp);
}

static void
ClipboardInitReadHistory(FcitxClipboard *clipboard)
{
    FILE *fp;
    fp = FcitxXDGGetFileUserWithPrefix("clipboard", "history.dat", "r", NULL);
    if (!fp)
        return;
    uint32_t len;
    fcitx_utils_read_uint32(fp, &len);
    fcitx_utils_read_uint32(fp, &clipboard->primary.len);
    int i;
    if (len > clipboard->config.history_len) {
        clipboard->clp_hist_len = clipboard->config.history_len;
    } else {
        clipboard->clp_hist_len = len;
    }
    for (i = 0;i < clipboard->clp_hist_len;i++) {
        fcitx_utils_read_uint32(fp, &clipboard->clp_hist_lst[i].len);
    }
    fseek(fp, (len + 2) * sizeof(uint32_t), SEEK_SET);
    clipboard->primary.str = malloc(clipboard->primary.len + 1);
    fread(clipboard->primary.str, 1, clipboard->primary.len, fp);
    clipboard->primary.str[clipboard->primary.len] = '\0';
    for (i = 0;i < clipboard->clp_hist_len;i++) {
        clipboard->clp_hist_lst[i].str = malloc(
            clipboard->clp_hist_lst[i].len + 1);
        fread(clipboard->clp_hist_lst[i].str, 1,
              clipboard->clp_hist_lst[i].len, fp);
        clipboard->clp_hist_lst[i].str[clipboard->clp_hist_lst[i].len] = '\0';
    }
    fclose(fp);
}

static INPUT_RETURN_VALUE
ClipboardCommitCallback(void *arg, FcitxCandidateWord *cand_word)
{
    FcitxClipboard *clipboard = arg;
    FcitxInstance *instance = clipboard->owner;
    char *commit = cand_word->priv ? cand_word->priv : cand_word->strWord;
    FcitxInstanceCommitString(instance, FcitxInstanceGetCurrentIC(instance),
                              commit);
    return IRV_FLAG_RESET_INPUT | IRV_FLAG_UPDATE_CANDIDATE_WORDS;
}

static boolean
ClipboardPreHook(void *arg, FcitxKeySym sym, unsigned int state,
                 INPUT_RETURN_VALUE *ret_val)
{
    FcitxClipboard *clipboard = arg;
    FcitxInstance *instance = clipboard->owner;
    if (!clipboard->active)
        return false;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    FcitxGlobalConfig* fc = FcitxInstanceGetGlobalConfig(instance);
    *ret_val = IRV_TO_PROCESS;

    const FcitxHotkey *prev = FcitxInstanceGetContextHotkey(
        instance, CONTEXT_ALTERNATIVE_PREVPAGE_KEY);
    if (prev == NULL)
        prev = fc->hkPrevPage;

    int key;
    const FcitxHotkey *next = FcitxInstanceGetContextHotkey(
        instance, CONTEXT_ALTERNATIVE_NEXTPAGE_KEY);
    if (next == NULL)
        next = fc->hkNextPage;

    if (FcitxHotkeyIsHotKey(sym, state, prev)) {
        if (FcitxCandidateWordGoPrevPage(cand_list))
            *ret_val = IRV_DISPLAY_MESSAGE;
    } else if (FcitxHotkeyIsHotKey(sym, state, next)) {
        if (FcitxCandidateWordGoNextPage(cand_list))
            *ret_val = IRV_DISPLAY_MESSAGE;
    } else if ((key = FcitxCandidateWordCheckChooseKey(cand_list,
                                                       sym, state)) >= 0) {
        *ret_val = FcitxCandidateWordChooseByIndex(cand_list, key);
    } else if (FcitxHotkeyIsHotKey(sym, state, FCITX_SPACE)) {
        if (FcitxCandidateWordPageCount(cand_list) != 0)
            *ret_val = FcitxCandidateWordChooseByIndex(cand_list, 0);
    }
    return true;
}

#define CLIPBOARD_CAND_LIMIT (127)
#define CLIPBOARD_CAND_HALF (60)
#define case_blank case ' ': case '\t': case '\b': case '\n': case '\f': \
case '\v': case '\r'
#define CLIPBOARD_CAND_SEP "  \xe2\x80\xa6  "

static char*
ClipboardSelectionStrip(const char *str, uint32_t len)
{
    const char *begin = str;
    const char *end = str + len;
    for (;begin < end;begin++) {
        switch (*begin) {
        case_blank:
            continue;
        default:
            break;
        }
        break;
    }
    for (;end >= begin;end--) {
        switch (*(end - 1)) {
        case_blank:
            continue;
        default:
            break;
        }
        break;
    }
    if (end <= begin)
        return strdup("");
    len = end - begin;
    char *res;
    char *p;
    if (len < CLIPBOARD_CAND_LIMIT) {
        res = malloc(len + 1);
        memcpy(res, begin, len);
        res[len] = '\0';
        goto out;
    }
    const char *begin_end = begin + CLIPBOARD_CAND_HALF;
    const char *end_begin = end - CLIPBOARD_CAND_HALF;
    for (;begin_end < end_begin;begin_end++) {
        if (fcitx_utf8_valid_start(*begin_end))
            break;
    }
    for (;begin_end < end_begin;end_begin--) {
        if (fcitx_utf8_valid_start(*end_begin))
            break;
    }
    int begin_len = begin_end - begin;
    int end_len = end - end_begin;
    res = malloc(begin_len + end_len + strlen(CLIPBOARD_CAND_SEP) + 1);
    p = res;
    memcpy(p, begin, begin_len);
    p += begin_len;
    memcpy(p, CLIPBOARD_CAND_SEP, strlen(CLIPBOARD_CAND_SEP));
    p += strlen(CLIPBOARD_CAND_SEP);
    memcpy(p, end_begin, end_len);
    p += end_len;
    *p = '\0';
out:
    for (p = res;*p;p++) {
        switch (*p) {
        case_blank:
            *p = ' ';
        }
    }
    return res;
}

static void
ClipboardSetCandWord(FcitxCandidateWord *cand_word, ClipboardSelectionStr *str)
{
    cand_word->strWord = ClipboardSelectionStrip(str->str, str->len);
    char *real = malloc(str->len + 1);
    cand_word->priv = real;
    memcpy(real, str->str, str->len);
    real[str->len] = '\0';
}

static boolean
ClipboardPostHook(void *arg, FcitxKeySym sym, unsigned int state,
                  INPUT_RETURN_VALUE *ret_val)
{
    FcitxClipboard *clipboard = arg;
    if (!(clipboard->primary.len && clipboard->clp_hist_len))
        return false;
    if (!FcitxHotkeyIsHotKey(sym, state, clipboard->config.trigger_key))
        return false;
    clipboard->active = true;
    FcitxInstance *instance = clipboard->owner;
    FcitxInputState *input = FcitxInstanceGetInputState(instance);
    FcitxCandidateWordList *cand_list = FcitxInputStateGetCandidateList(input);
    FcitxCandidateWord cand_word = {
        .callback = ClipboardCommitCallback,
        .wordType = MSG_OTHER,
        .owner = clipboard
    };
    FcitxCandidateWordReset(cand_list);
    if (clipboard->clp_hist_len) {
        ClipboardSetCandWord(&cand_word, clipboard->clp_hist_lst);
        FcitxCandidateWordAppend(cand_list, &cand_word);
    }
    if (clipboard->primary.len) {
        ClipboardSetCandWord(&cand_word, &clipboard->primary);
        FcitxCandidateWordAppend(cand_list, &cand_word);
    }
    int i;
    for (i = 1;i < clipboard->clp_hist_len;i++) {
        ClipboardSetCandWord(&cand_word, clipboard->clp_hist_lst + i);
        FcitxCandidateWordAppend(cand_list, &cand_word);
    }
    *ret_val = IRV_FLAG_UPDATE_INPUT_WINDOW;
    return true;
}

static void
ClipboardReset(void *arg)
{
    FcitxClipboard *clipboard = arg;
    clipboard->active = false;
}

static void*
ClipboardCreate(FcitxInstance *instance)
{
    FcitxClipboard *clipboard = fcitx_utils_new(FcitxClipboard);
    clipboard->owner = instance;

    if (!FcitxClipboardLoadConfig(&clipboard->config)) {
        ClipboardDestroy(clipboard);
        return NULL;
    }
    ClipboardInitReadHistory(clipboard);
    ClipboardInitX11(clipboard);
    ApplyClipboardConfig(clipboard);

    FcitxKeyFilterHook key_hook = {
        .arg = clipboard,
        .func = ClipboardPreHook
    };
    FcitxInstanceRegisterPreInputFilter(instance, key_hook);

    key_hook.func = ClipboardPostHook;
    FcitxInstanceRegisterPostInputFilter(instance, key_hook);

    FcitxIMEventHook reset_hook = {
        .arg = clipboard,
        .func = ClipboardReset
    };
    FcitxInstanceRegisterResetInputHook(instance, reset_hook);

    return clipboard;
}

static void
ClipboardDestroy(void *arg)
{
    FcitxClipboard *clipboard = (FcitxClipboard*)arg;
    ClipboardWriteHistory(clipboard);
    fcitx_utils_free(clipboard->primary.str);
    free(arg);
}

static void
ApplyClipboardConfig(FcitxClipboard *clipboard)
{
    FcitxClipboardConfig *config = &clipboard->config;
    if (config->history_len < 1) {
        config->history_len = 1;
    } else if (config->history_len > CLIPBOARD_MAX_LEN) {
        config->history_len = CLIPBOARD_MAX_LEN;
    }
    while (clipboard->clp_hist_len > config->history_len) {
        char *str = clipboard->clp_hist_lst[--clipboard->clp_hist_len].str;
        fcitx_utils_free(str);
    }
    ClipboardWriteHistory(clipboard);
}

static void
ClipboardReloadConfig(void* arg)
{
    FcitxClipboard *clipboard = (FcitxClipboard*)arg;
    FcitxClipboardLoadConfig(&clipboard->config);
    ApplyClipboardConfig(clipboard);
}

void
ClipboardSetPrimary(FcitxClipboard *clipboard, uint32_t len, const char *str)
{
    if (!(len && str))
        return;
    clipboard->primary.str = realloc(clipboard->primary.str, len + 1);
    memcpy(clipboard->primary.str, str, len);
    clipboard->primary.str[len] = '\0';
    clipboard->primary.len = len;
}

void
ClipboardPushClipboard(FcitxClipboard *clipboard, uint32_t len, const char *str)
{
    if (!(len && str))
        return;
    if (clipboard->clp_hist_len &&
        len == clipboard->clp_hist_lst->len &&
        !memcmp(clipboard->clp_hist_lst->str, str, len))
        return;
    char *new_str;
    if (clipboard->clp_hist_len < clipboard->config.history_len) {
        clipboard->clp_hist_len++;
        new_str = NULL;
    } else {
        new_str = clipboard->clp_hist_lst[clipboard->clp_hist_len - 1].str;
    }
    memmove(clipboard->clp_hist_lst + 1, clipboard->clp_hist_lst,
            (clipboard->clp_hist_len - 1) * sizeof(ClipboardSelectionStr));
    new_str = realloc(new_str, len + 1);
    memcpy(new_str, str, len);
    new_str[len] = '\0';
    clipboard->clp_hist_lst->len = len;
    clipboard->clp_hist_lst->str = new_str;
}
