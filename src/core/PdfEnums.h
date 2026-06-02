// SPDX-License-Identifier: Apache-2.0
#pragma once

enum class ToolMode {
    HandTool,
    SelectText,
    EditText,
    EditObject,
    Highlight,
    Underline,
    Strikeout,
    Squiggly,
    DrawShape,
    DrawFreehand,
    AddTextBox,
    AddComment,
    Redact,
    AddSignature,
    DrawRectangle,
    DrawEllipse,
    DrawLine,
    DrawArrow,
    AddTextField,
    AddCheckbox,
    EditImage,
    Stamp,
    Callout,
    Crop,
    // Form builder field placement modes (M3-PROMPT-1)
    FormAddText,
    FormAddCheckbox,
    FormAddRadio,
    FormAddDropdown,
    FormAddListBox,
    FormAddDate,
    FormAddNumeric,
    FormAddSignature,
    FormAddButton
};
