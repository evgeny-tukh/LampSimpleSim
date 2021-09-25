#include <Windows.h>
#include <cstdint>
#include "resource.h"

struct EditCtx {
    char *buffer, *prompt, *caption;
    size_t size;
};

INT_PTR editBoxProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    switch (msg) {
        case WM_COMMAND: {
            switch (LOWORD (param1)) {
                case ID_OK: {
                    EditCtx *ctx = (EditCtx *) GetWindowLongPtr (wnd, GWLP_USERDATA);
                    GetDlgItemText (wnd, ID_EDITING_TEXT, ctx->buffer, ctx->size);
                    EndDialog (wnd, ID_OK);
                    return 1;
                }
                case ID_CANCEL: {
                    EndDialog (wnd, ID_CANCEL); return 1;
                }
            }
            return 1;
        }
        case WM_INITDIALOG: {
            EditCtx *ctx = (EditCtx *) param2;

            SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) param2);
            SetWindowText (wnd, ctx->caption);
            SetDlgItemText (wnd, ID_PROMPT, ctx->prompt);
            SetDlgItemText (wnd, ID_EDITING_TEXT, ctx->buffer);
            return 1;
        }
        default: {
            return 0;
        }
    }
}

bool editText (HWND parent, HINSTANCE instance, char *caption, char *prompt, char *buffer, size_t size, char *initValue) {
    EditCtx ctx { buffer, prompt, caption, size };

    memset (buffer, 0, size);
    
    if (initValue) strcpy (buffer, initValue);

    ctx.prompt = prompt;
    ctx.caption = caption;

    return DialogBoxParam (instance, MAKEINTRESOURCE (IDD_EDITBOX), parent, editBoxProc, (LPARAM) & ctx) == ID_OK;
}

bool editNumber (HWND parent, HINSTANCE instance, char *caption, char *prompt, uint32_t *value, uint32_t initValue) {
    char buffer [50], initString [50];
    bool result;

    _itoa (initValue, initString, 10);

    result = editText (parent, instance, caption, prompt, buffer, sizeof (buffer), initString);

    if (result && value) *value = atoi (buffer);
    
    return result;
}
