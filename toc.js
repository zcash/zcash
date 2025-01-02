// Populate the sidebar
//
// This is a script, and not included directly in the page, to control the total size of the book.
// The TOC contains an entry for each page, so if each page includes a copy of the TOC,
// the total size of the page becomes O(n**2).
class MDBookSidebarScrollbox extends HTMLElement {
    constructor() {
        super();
    }
    connectedCallback() {
        this.innerHTML = '<ol class="chapter"><li class="chapter-item expanded affix "><a href="index.html">zcashd</a></li><li class="chapter-item expanded "><a href="user.html"><strong aria-hidden="true">1.</strong> User Documentation</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="user/release-support.html"><strong aria-hidden="true">1.1.</strong> Release Support</a></li><li class="chapter-item expanded "><a href="user/platform-support.html"><strong aria-hidden="true">1.2.</strong> Platform Support</a></li><li class="chapter-item expanded "><a href="user/wallet-backup.html"><strong aria-hidden="true">1.3.</strong> Wallet Backup</a></li><li class="chapter-item expanded "><a href="user/shield-coinbase.html"><strong aria-hidden="true">1.4.</strong> Shielding Coinbase Outputs</a></li><li class="chapter-item expanded "><a href="user/files.html"><strong aria-hidden="true">1.5.</strong> Data Directory Structure</a></li><li class="chapter-item expanded "><a href="user/metrics.html"><strong aria-hidden="true">1.6.</strong> Metrics</a></li><li class="chapter-item expanded "><a href="user/tor.html"><strong aria-hidden="true">1.7.</strong> Using zcashd with Tor</a></li><li class="chapter-item expanded "><a href="user/security-warnings.html"><strong aria-hidden="true">1.8.</strong> Security Warnings</a></li><li class="chapter-item expanded "><a href="user/deprecation.html"><strong aria-hidden="true">1.9.</strong> Deprecated Features</a></li></ol></li><li class="chapter-item expanded "><a href="dev.html"><strong aria-hidden="true">2.</strong> Developer Documentation</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="dev/dnsseed-policy.html"><strong aria-hidden="true">2.1.</strong> DNS Seeders</a></li><li class="chapter-item expanded "><a href="dev/rust.html"><strong aria-hidden="true">2.2.</strong> Rust in zcashd</a></li><li class="chapter-item expanded "><a href="dev/regtest.html"><strong aria-hidden="true">2.3.</strong> Regtest Tips And Hints</a></li><li class="chapter-item expanded "><a href="dev/platform-tier-policy.html"><strong aria-hidden="true">2.4.</strong> Platform Tier Policy</a></li><li class="chapter-item expanded "><a href="dev/deprecation.html"><strong aria-hidden="true">2.5.</strong> Deprecation Procedure</a></li></ol></li><li class="chapter-item expanded "><a href="design.html"><strong aria-hidden="true">3.</strong> Design</a></li><li><ol class="section"><li class="chapter-item expanded "><a href="design/chain-state.html"><strong aria-hidden="true">3.1.</strong> Chain State</a></li><li class="chapter-item expanded "><a href="design/coins-view.html"><strong aria-hidden="true">3.2.</strong> "Coins" View</a></li><li class="chapter-item expanded "><a href="design/p2p-data-propagation.html"><strong aria-hidden="true">3.3.</strong> P2P Data Propagation</a></li></ol></li></ol>';
        // Set the current, active page, and reveal it if it's hidden
        let current_page = document.location.href.toString();
        if (current_page.endsWith("/")) {
            current_page += "index.html";
        }
        var links = Array.prototype.slice.call(this.querySelectorAll("a"));
        var l = links.length;
        for (var i = 0; i < l; ++i) {
            var link = links[i];
            var href = link.getAttribute("href");
            if (href && !href.startsWith("#") && !/^(?:[a-z+]+:)?\/\//.test(href)) {
                link.href = path_to_root + href;
            }
            // The "index" page is supposed to alias the first chapter in the book.
            if (link.href === current_page || (i === 0 && path_to_root === "" && current_page.endsWith("/index.html"))) {
                link.classList.add("active");
                var parent = link.parentElement;
                if (parent && parent.classList.contains("chapter-item")) {
                    parent.classList.add("expanded");
                }
                while (parent) {
                    if (parent.tagName === "LI" && parent.previousElementSibling) {
                        if (parent.previousElementSibling.classList.contains("chapter-item")) {
                            parent.previousElementSibling.classList.add("expanded");
                        }
                    }
                    parent = parent.parentElement;
                }
            }
        }
        // Track and set sidebar scroll position
        this.addEventListener('click', function(e) {
            if (e.target.tagName === 'A') {
                sessionStorage.setItem('sidebar-scroll', this.scrollTop);
            }
        }, { passive: true });
        var sidebarScrollTop = sessionStorage.getItem('sidebar-scroll');
        sessionStorage.removeItem('sidebar-scroll');
        if (sidebarScrollTop) {
            // preserve sidebar scroll position when navigating via links within sidebar
            this.scrollTop = sidebarScrollTop;
        } else {
            // scroll sidebar to current active section when navigating via "next/previous chapter" buttons
            var activeSection = document.querySelector('#sidebar .active');
            if (activeSection) {
                activeSection.scrollIntoView({ block: 'center' });
            }
        }
        // Toggle buttons
        var sidebarAnchorToggles = document.querySelectorAll('#sidebar a.toggle');
        function toggleSection(ev) {
            ev.currentTarget.parentElement.classList.toggle('expanded');
        }
        Array.from(sidebarAnchorToggles).forEach(function (el) {
            el.addEventListener('click', toggleSection);
        });
    }
}
window.customElements.define("mdbook-sidebar-scrollbox", MDBookSidebarScrollbox);
