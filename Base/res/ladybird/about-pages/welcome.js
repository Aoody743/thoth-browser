/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

import { populateSearchEngineSelect, updateSearchSuggestionsControl } from "./search-engine-controls.js";

const intro = document.querySelector("#welcome-intro");
const form = document.querySelector("#welcome-form");
const setupOptions = document.querySelector("#setup-options");
const setupSteps = [...document.querySelectorAll(".setup-step")];
const beginSetup = document.querySelector("#begin-setup");
const previousStep = document.querySelector("#previous-step");
const continueStep = document.querySelector("#continue-step");
const startBrowsing = document.querySelector("#start-browsing");
const stepCount = document.querySelector("#step-count");
const stepProgress = document.querySelector("#step-progress");
const progressSegments = [...stepProgress.querySelectorAll(".progress-segment")];
const searchEngine = document.querySelector("#search-engine");
const searchSuggestions = document.querySelector("#search-suggestions-enabled");
const searchSuggestionsDescription = document.querySelector("#search-suggestions-description");
const enableContentBlocking = document.querySelector("#enable-content-blocking");
const blockingOptions = document.querySelector("#blocking-options");
const contentBlockerLists = document.querySelector("#content-blocker-lists");
const saveStatus = document.querySelector("#save-status");

let settings;
let engines;
let initialized = false;
let activeStep = -1;

const filterListDescriptions = {
    easyList: "Blocks most ads.",
    easyPrivacy: "Blocks trackers.",
    fanboyAnnoyances: "Blocks pop-ups and other annoyances.",
};

function updateBlockingOptions() {
    blockingOptions.classList.toggle("hidden", !enableContentBlocking.checked);
}

function showStep(index) {
    if (index < 0) {
        activeStep = -1;
        form.hidden = true;
        intro.hidden = false;
        beginSetup.focus();
        return;
    }

    activeStep = Math.min(index, setupSteps.length - 1);
    intro.hidden = true;
    form.hidden = false;
    setupSteps.forEach((step, stepIndex) => {
        step.hidden = stepIndex !== activeStep;
        step.classList.remove("is-entering");
    });
    const currentStep = setupSteps[activeStep];
    requestAnimationFrame(() => {
        currentStep.classList.add("is-entering");
        window.setTimeout(() => currentStep.classList.remove("is-entering"), 450);
    });

    stepCount.innerHTML = `${String(activeStep + 1).padStart(2, "0")} <span>/ 0${setupSteps.length}</span>`;
    stepProgress.setAttribute("aria-valuenow", String(activeStep + 1));
    progressSegments.forEach((segment, segmentIndex) => segment.classList.toggle("active", segmentIndex <= activeStep));
    continueStep.hidden = activeStep === setupSteps.length - 1;
    startBrowsing.hidden = activeStep !== setupSteps.length - 1;
    saveStatus.textContent = "";
    currentStep.querySelector("h2")?.focus({ preventScroll: true });
}

function initialize() {
    if (initialized || !settings || !engines) return;
    initialized = true;

    const savedLanguage = settings.interfaceLanguage === "zh-CN" ? "zh-CN" : "en";
    const languageOption = form.querySelector(`input[name="interface-language"][value="${savedLanguage}"]`);
    if (languageOption) languageOption.checked = true;
    document.dispatchEvent(new CustomEvent("ThothInterfaceLanguagePreview", { detail: savedLanguage }));

    populateSearchEngineSelect(searchEngine, engines.search, settings.searchEngine);
    searchSuggestions.checked = settings.searchEngine.suggestions;
    updateSearchSuggestionsControl(searchEngine, searchSuggestions, searchSuggestionsDescription);

    enableContentBlocking.checked = settings.contentBlockers.enabled;
    for (const list of settings.contentBlockerLists) {
        if (!Object.hasOwn(filterListDescriptions, list.identifier)) continue;

        const row = document.createElement("label");
        row.className = "filter-list";
        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.dataset.identifier = list.identifier;
        checkbox.checked = list.enabled;
        const name = document.createElement("span");
        name.textContent = list.name;
        const description = document.createElement("p");
        description.id = `description-${list.identifier}`;
        description.textContent = filterListDescriptions[list.identifier];
        checkbox.setAttribute("aria-describedby", description.id);
        row.append(checkbox, name, description);
        contentBlockerLists.append(row);
    }

    updateBlockingOptions();
    setupOptions.disabled = false;
    beginSetup.disabled = false;
    continueStep.disabled = false;
    startBrowsing.disabled = false;
}

beginSetup.addEventListener("click", () => showStep(0));
form.addEventListener("change", event => {
    if (event.target.name === "interface-language")
        document.dispatchEvent(new CustomEvent("ThothInterfaceLanguagePreview", { detail: event.target.value }));
});
previousStep.addEventListener("click", () => showStep(activeStep - 1));
continueStep.addEventListener("click", () => showStep(activeStep + 1));
enableContentBlocking.addEventListener("change", updateBlockingOptions);
searchEngine.addEventListener("change", () => {
    updateSearchSuggestionsControl(searchEngine, searchSuggestions, searchSuggestionsDescription);
});

form.addEventListener("submit", event => {
    event.preventDefault();
    if (!initialized || startBrowsing.disabled) return;

    setupOptions.disabled = true;
    continueStep.disabled = true;
    startBrowsing.disabled = true;
    saveStatus.textContent = "Saving your choices…";
    ladybird.sendMessage("completeFirstRun", {
        verticalTabsEnabled: true,
        interfaceLanguage: form.querySelector('input[name="interface-language"]:checked')?.value || "en",
        searchEngine: { engine: searchEngine.value || null, suggestions: searchSuggestions.checked },
        contentBlockerEnabled: enableContentBlocking.checked,
        contentBlockerLists: [...contentBlockerLists.querySelectorAll("input")].map(checkbox => ({
            identifier: checkbox.dataset.identifier,
            enabled: checkbox.checked,
        })),
    });
});

document.addEventListener("WebUILoaded", () => {
    ladybird.sendMessage("loadAvailableEngines");
    ladybird.sendMessage("loadCurrentSettings");
});

document.addEventListener("WebUIMessage", event => {
    const { name, data } = event.detail;
    if (name === "loadSettings") settings = data;
    else if (name === "loadEngines") engines = data;
    else if (name === "firstRunCompleted") location.replace(data);
    else if (name === "firstRunError") {
        saveStatus.textContent = data;
        setupOptions.disabled = false;
        continueStep.disabled = false;
        startBrowsing.disabled = false;
    }
    initialize();
});
