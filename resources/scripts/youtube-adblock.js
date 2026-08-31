(() => {
    "use strict";

    const host = location.hostname.toLowerCase();
    if (host !== "youtube.com" && !host.endsWith(".youtube.com")) return;
    if (globalThis.__virginYouTubeAdBlock) return;
    globalThis.__virginYouTubeAdBlock = true;

    const hiddenSelectors = [
        "#masthead-ad",
        "#player-ads",
        "ytd-ad-slot-renderer",
        "ytd-display-ad-renderer",
        "ytd-in-feed-ad-layout-renderer",
        "ytd-promoted-sparkles-web-renderer",
        "ytd-banner-promo-renderer",
        "ytd-companion-slot-renderer",
        "ytm-companion-ad-renderer",
        "ad-slot-renderer",
        ".ytp-ad-overlay-container",
        ".ytp-ad-player-overlay",
        ".ytp-ad-image-overlay",
        ".ytp-ad-text-overlay"
    ];

    const skipSelectors = [
        ".ytp-ad-skip-button-modern",
        ".ytp-ad-skip-button",
        ".ytp-skip-ad-button",
        ".videoAdUiSkipButton",
        "button[aria-label^='Skip ad']",
        "button[aria-label^='Skip Ad']"
    ];

    let activeVideo = null;
    let savedMediaState = null;
    let scanQueued = false;

    function installStyle() {
        if (document.getElementById("virgin-youtube-ad-style")) return;
        const style = document.createElement("style");
        style.id = "virgin-youtube-ad-style";
        style.textContent = `${hiddenSelectors.join(",")} { display: none !important; }`;
        (document.head || document.documentElement).appendChild(style);
    }

    function adIsPlaying(player) {
        return Boolean(player &&
            (player.classList.contains("ad-showing") ||
             player.classList.contains("ad-interrupting")));
    }

    function restoreVideo() {
        if (!activeVideo || !savedMediaState) return;
        try {
            activeVideo.muted = savedMediaState.muted;
            activeVideo.volume = savedMediaState.volume;
            activeVideo.playbackRate = savedMediaState.playbackRate;
        } catch (_) {
            // The player may have replaced the media element during navigation.
        }
        activeVideo = null;
        savedMediaState = null;
    }

    function skipCurrentAd() {
        installStyle();
        const player = document.querySelector("#movie_player, .html5-video-player");
        if (!adIsPlaying(player)) {
            restoreVideo();
            return;
        }

        for (const selector of skipSelectors) {
            const button = document.querySelector(selector);
            if (button instanceof HTMLElement && button.offsetParent !== null) {
                button.click();
                return;
            }
        }

        const closeButton = document.querySelector(".ytp-ad-overlay-close-button");
        if (closeButton instanceof HTMLElement) closeButton.click();

        const video = player ? player.querySelector("video") : null;
        if (!(video instanceof HTMLVideoElement)) return;
        if (activeVideo !== video) {
            restoreVideo();
            activeVideo = video;
            savedMediaState = {
                muted: video.muted,
                volume: video.volume,
                playbackRate: video.playbackRate
            };
        }

        video.muted = true;
        const duration = video.duration;
        if (Number.isFinite(duration) && duration > 0 && duration < 600) {
            try {
                video.currentTime = Math.max(0, duration - 0.05);
            } catch (_) {
                video.playbackRate = 16;
            }
        } else {
            video.playbackRate = 16;
        }
    }

    function queueScan() {
        if (scanQueued) return;
        scanQueued = true;
        queueMicrotask(() => {
            scanQueued = false;
            skipCurrentAd();
        });
    }

    function start() {
        installStyle();
        skipCurrentAd();
        new MutationObserver(queueScan).observe(document.documentElement, {
            childList: true,
            subtree: true
        });
        document.addEventListener("yt-navigate-finish", queueScan, true);
        setInterval(skipCurrentAd, 400);
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", start, {once: true});
    } else {
        start();
    }
})();
