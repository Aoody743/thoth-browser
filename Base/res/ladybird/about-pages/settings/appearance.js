const showMenuBar = document.querySelector("#show-menu-bar");
const showMenuBarGroup = document.querySelector("#show-menu-bar-group");
const showBookmarksBar = document.querySelector("#show-bookmarks-bar");
const colorScheme = document.querySelector("#color-scheme");
const browserUIFont = document.querySelector("#browser-ui-font");
const browserIconWeight = document.querySelector("#browser-icon-weight");
const browserPalettes = document.querySelector("#browser-color-palettes");

let APPEARANCE = {};

const PALETTE_ACCENTS = {
    lavender: { light: "#7866a6", dark: "#c1aceb" },
    ocean: { light: "#4f778f", dark: "#91c0d5" },
    sage: { light: "#577b66", dark: "#9dc5a8" },
    rose: { light: "#9a657a", dark: "#dca5b8" },
    sand: { light: "#92713f", dark: "#dbba7c" },
    graphite: { light: "#687382", dark: "#bdc6d3" },
    plum: { light: "#80577f", dark: "#d0a6cf" },
    terracotta: { light: "#a65f48", dark: "#e5a18a" },
};

function setPaletteAccent(palette) {
    const accents = PALETTE_ACCENTS[palette] || PALETTE_ACCENTS.lavender;
    const dark = window.matchMedia?.("(prefers-color-scheme: dark)").matches;
    document.documentElement.style.setProperty("--thoth-accent", dark ? accents.dark : accents.light);
}

const loadFeatures = features => {
    showMenuBarGroup.classList.toggle("hidden", !features?.menuBar);
};

const loadSettings = settings => {
    APPEARANCE = settings.appearance || {};

    showMenuBar.checked = !!APPEARANCE.showMenuBar;
    showBookmarksBar.checked = !!APPEARANCE.showBookmarksBar;
    colorScheme.value = APPEARANCE.colorScheme || "system";
    browserUIFont.value = APPEARANCE.browserUiFont || "system";
    browserIconWeight.value = APPEARANCE.browserIconWeight || "balanced";
    const palette = PALETTE_ACCENTS[APPEARANCE.browserColorPalette] ? APPEARANCE.browserColorPalette : "lavender";
    APPEARANCE.browserColorPalette = palette;
    setSelectedPalette(palette);
    setPaletteAccent(palette);
};

function setSelectedPalette(palette) {
    for (const choice of browserPalettes.querySelectorAll("[data-palette]")) {
        choice.setAttribute("aria-checked", String(choice.dataset.palette === palette));
    }
}

function addChangeHandler(input, name) {
    input.addEventListener("change", () => {
        APPEARANCE[name] = input.checked;
        ladybird.sendMessage("setAppearance", APPEARANCE);
    });
}

addChangeHandler(showMenuBar, "showMenuBar");
addChangeHandler(showBookmarksBar, "showBookmarksBar");

colorScheme.addEventListener("change", () => {
    APPEARANCE.colorScheme = colorScheme.value;
    ladybird.sendMessage("setAppearance", APPEARANCE);
});

browserUIFont.addEventListener("change", () => {
    APPEARANCE.browserUiFont = browserUIFont.value;
    ladybird.sendMessage("setAppearance", APPEARANCE);
});

browserIconWeight.addEventListener("change", () => {
    APPEARANCE.browserIconWeight = browserIconWeight.value;
    ladybird.sendMessage("setAppearance", APPEARANCE);
});

browserPalettes.addEventListener("click", event => {
    const choice = event.target.closest("[data-palette]");
    if (!choice)
        return;

    APPEARANCE.browserColorPalette = choice.dataset.palette;
    setSelectedPalette(choice.dataset.palette);
    setPaletteAccent(choice.dataset.palette);
    ladybird.sendMessage("setAppearance", APPEARANCE);
});

window.matchMedia?.("(prefers-color-scheme: dark)").addEventListener("change", () => {
    setPaletteAccent(APPEARANCE.browserColorPalette || "lavender");
});

document.addEventListener("WebUIMessage", event => {
    if (event.detail.name === "loadFeatures") {
        loadFeatures(event.detail.data);
    } else if (event.detail.name === "loadSettings") {
        loadSettings(event.detail.data);
    }
});
