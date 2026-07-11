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
    // §9.2 Wave 1B: real eraser mode -- a click hit-tests page content at the
    // clicked point and excises it via IPageEditor::deleteObjectAt (the same
    // pipeline MarkRedact/applyRedactions already uses). Replaces the former
    // "not yet implemented" placeholder in EditController::activate(Erase).
    Erase,
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
    FormAddButton,
    FormAddCalculated
};
