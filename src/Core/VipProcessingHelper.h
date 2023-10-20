/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VIP_PROCESSING_HELPER
#define VIP_PROCESSING_HELPER

/// @brief Defines a new input/output/property for a VipProcessingObject.
/// This could be done with something like Q_PROPERTY(VipInput input), but this generates a moc warning
/// as the property does not have a valid READ member.
/// Using VIP_IO binds the input to a dummy READ function that remove the moc warnings.
#define VIP_IO(...) Q_PROPERTY(__VA_ARGS__ READ io)

/// @brief Defines the category of the VipProcessingObject.
///
/// Shortcup for Q_CLASSINFO("category","my category")
#define VIP_CATEGORY(...) Q_CLASSINFO("category", __VA_ARGS__)

/// @brief Defines the description of the VipProcessingObject.
///
/// Shortcup for Q_CLASSINFO("description","my description")
#define VIP_DESCRIPTION(...) Q_CLASSINFO("description", __VA_ARGS__)

/// @brief Defines the description of VipInput, VipOutput, VipProperty or VipMulti..  of the VipProcessingObject.
///
/// Shortcup for Q_CLASSINFO("MyIOName","my description")
#define VIP_IO_DESCRIPTION(name, ...) Q_CLASSINFO(#name, __VA_ARGS__)

/// @brief Defines the editor widget that will be used to edit a VipProperty of a VipProcessingObject.
///
/// Shortcup for Q_CLASSINFO("edit_MyPropertyName","editor style sheet").
/// The editor widget is defined through its Qt style sheet. The editor widget must follow 3 rules:
/// <ul>
/// <li>It defines the 'value' property</li>
/// <li>It defines the "void genericValueCHanged(const QVariant&)" signal</li>
/// <li>It is declared with VIP_REGISTER_QOBJECT_METATYPE.</li>
/// </ul>
/// There are currently several editors defined in Thermavip (Gui library), like #VipLineEdit, VipDoubleEdit, etc.
/// There are also several macros that help you builing the style sheet for each editor (see for instance #VIP_LINE_EDIT or #VIP_DOUBLE_EDIT).
#define VIP_PROPERTY_EDIT(name, ...) Q_CLASSINFO("edit_" #name, __VA_ARGS__)

/// @brief Defines the category of a VipProperty of a VipProcessingObject.
///
/// Shortcup for Q_CLASSINFO("category_MyPropertyName","MyCategory/MySubCategory").
#define VIP_PROPERTY_CATEGORY(name, ...) Q_CLASSINFO("category_" #name, __VA_ARGS__)

/// @brief Builds the style sheet to create a VipSpinBox widget
#define VIP_SPINBOX_EDIT(min, max, step, value) "VipSpinBox{  qproperty-minimum:" #min "; qproperty-maximum:" #max "; qproperty-singleStep:" #step "; qproperty-value:" #value ";}"
/// @brief Builds the style sheet to create a VipDoubleSpinBox widget
#define VIP_DOUBLE_SPINBOX_EDIT(min, max, step, value) "VipDoubleSpinBox{ qproperty-minimum:" #min "; qproperty-maximum:" #max "; qproperty-singleStep:" #step "; qproperty-value:" #value ";}"
/// @brief Builds the style sheet to create a VipDoubleSliderEdit widget
#define VIP_DOUBLE_SLIDER_EDIT(min, max, step, value, show_spin_box)                                                                                                                                   \
	"VipDoubleSliderEdit{ qproperty-minimum:" #min "; qproperty-maximum:" #max "; qproperty-singleStep:" #step "; qproperty-value:" #value "; qproperty-showSpinBox:" #show_spin_box ";}"
/// @brief Builds the style sheet to create a VipDoubleEdit widget
#define VIP_DOUBLE_EDIT(value, d_format) "VipDoubleEdit{ qproperty-value:" #value "; qproperty-format:" #d_format ";}"
/// @brief Builds the style sheet to create a VipComboBox widget
#define VIP_COMBOBOX_EDIT(choices, value) "VipComboBox{ qproperty-choices:'" choices "'; qproperty-value:'" value "' ;}"
/// @brief Builds the style sheet to create a VipLineEdit widget
#define VIP_LINE_EDIT(value) "VipLineEdit{ qproperty-value:'" value "' ;}"
/// @brief Builds the style sheet to create a VipBrushWidget widget
#define VIP_BRUSH_EDIT(color) "VipBrushWidget{ qproperty-value:'" color "' ;}"
/// @brief Builds the style sheet to create a VipPenWidget widget
#define VIP_PEN_EDIT(brush, width, style, cap, join)                                                                                                                                                   \
	"VipPenWidget{ qproperty-brush:'" brush "' ;qproperty-width:" #width " ;qproperty-style:" #style " ;qproperty-cap:" #cap " ;qproperty-join:" #join " ;}"
/// @brief Builds the style sheet to create a VipFileName widget
#define VIP_FILENAME_EDIT(mode, filename, title, filters) "VipFileName{  qproperty-mode:'" mode "'; qproperty-value:'" filename "' ;qproperty-title:'" title "' ;qproperty-filters:'" filters "' ;}";
/// @brief Builds the style sheet to create a VipEnumEdit widget
#define VIP_ENUM_EDIT(enums, values, value) "VipEnumEdit{ qproperty-enumNames:'" enums "'; qproperty-enumValues:'" values "'; qproperty-value:" #value " ;}"

#endif
