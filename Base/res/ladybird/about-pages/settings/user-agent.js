const userAgentPreset = document.querySelector("#user-agent-preset");

function loadUserAgentSettings(settings) {
    const presets = settings.userAgentPresets || [];
    userAgentPreset.replaceChildren(new Option("Default", ""));
    presets.forEach(preset => {
        userAgentPreset.append(new Option(preset.name, preset.name));
    });
    userAgentPreset.value = settings.userAgentPreset || "";
}

userAgentPreset.addEventListener("change", () => {
    ladybird.sendMessage("setUserAgentPreset", userAgentPreset.value);
});

document.addEventListener("WebUIMessage", event => {
    if (event.detail.name === "loadSettings")
        loadUserAgentSettings(event.detail.data);
});
