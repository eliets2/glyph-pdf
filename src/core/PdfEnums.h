// SPDX-License-Identifier: Apache-2.0
#pragma once

enum class ToolMode {
    HandTool,
    SelectText,
    EditText,
    EditObject,
    Erase,
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
    FormAddButton,
    FormAddCalculated,
    // §9.7 P0 (audit 2026-07-01): typed-font and image-upload signature modes
    // of the Draw/Type/Upload picker. Appended AFTER the form-builder block so
    // existing ToolMode ordinals (persisted by AnnotationSerializer sidecars)
    // stay stable — never insert values here.
    AddSignatureTyped,
    AddSignatureUpload
};
