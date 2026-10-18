/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

const interfaceLanguage = document.querySelector("#interface-language");

interfaceLanguage.addEventListener("change", () => {
    const language = interfaceLanguage.value === "zh-CN" ? "zh-CN" : "en";
    document.dispatchEvent(new CustomEvent("ThothInterfaceLanguagePreview", { detail: language }));
    ladybird.sendMessage("setInterfaceLanguage", { interfaceLanguage: language });
});

document.addEventListener("WebUIMessage", event => {
    if (event.detail.name === "loadSettings")
        interfaceLanguage.value = event.detail.data.interfaceLanguage === "zh-CN" ? "zh-CN" : "en";
    else if (event.detail.name === "interfaceLanguageChanged")
        interfaceLanguage.value = event.detail.data === "zh-CN" ? "zh-CN" : "en";
});
