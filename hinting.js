/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2010 by Hans-Peter Deifel
    see LICENSE file
*/
function Hints() {
    var hintContainer;
    var currentFocusNum = 1;

    /* hints[] = [elem, number, text, span, backgroundColor, color] */
    var hints;

    this.createHints = function(inputText)
    {
        var topwin = window;
        var top_height = topwin.innerHeight;
        var top_width = topwin.innerWidth;
        var xpath_expr;

        var hintCount = 0;
        hints = [];

        function helper (win, offsetX, offsetY) {
            var doc = win.document;

            var win_height = win.height;
            var win_width = win.width;

            /* Bounds */
            var minX = offsetX < 0 ? -offsetX : 0;
            var minY = offsetY < 0 ? -offsetY : 0;
            var maxX = offsetX + win_width > top_width ? top_width - offsetX : top_width;
            var maxY = offsetY + win_height > top_height ? top_height - offsetY : top_height;

            var scrollX = win.scrollX;
            var scrollY = win.scrollY;

            hintContainer = doc.createElement("div");
            hintContainer.id = "hint_container";

            if (typeof(inputText) == "undefined" || inputText == "") {
                xpath_expr = "//*[@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href] | //input[not(@type='hidden')] | //a | //area | //textarea | //button | //select";
            } else {
                xpath_expr = "//*[(@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href) and contains(., '" + inputText + "')] | //input[not(@type='hidden') and contains(., '" + inputText + "')] | //a[contains(., '" + inputText + "')] | //area[contains(., '" + inputText + "')] | //iframe[contains(@name, '" + inputText + "')] | //textarea[contains(., '" + inputText + "')] | //button[contains(@value, '" + inputText + "')] | //select[contains(., '" + inputText + "')]";
            }

            var res = doc.evaluate(xpath_expr, doc,
                function (p) {
                    return "http://www.w3.org/1999/xhtml";
                }, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

            /* generate basic hint element which will be cloned and updated later */
            var hintSpan = doc.createElement("span");
            hintSpan.setAttribute("class", "hinting_mode_hint");
            hintSpan.style.position = "absolute";
            hintSpan.style.background = "red";
            hintSpan.style.color = "#fff";
            hintSpan.style.font = "bold 10px monospace";
            hintSpan.style.zIndex = "10000000";

            /* due to the different XPath result type, we will need two counter variables */
            var rect, elem, text, node, show_text;
            for (var i = 0; i < res.snapshotLength; i++)
            {
                elem = res.snapshotItem(i);
                rect = elem.getBoundingClientRect();
                if (!rect || rect.left > maxX || rect.right < minX || rect.top > maxY || rect.bottom < minY)
                    continue;

                var style = topwin.getComputedStyle(elem, "");
                if (style.display == "none" || style.visibility != "visible")
                    continue;

                var leftpos = Math.max((rect.left + scrollX), scrollX);
                var toppos = Math.max((rect.top + scrollY), scrollY);

                /* process elements text */
                text = _getTextFromElement(elem);

                /* making this block DOM compliant */
                var hint = hintSpan.cloneNode(false);
                hint.setAttribute("id", "vimprobablehint" + hintCount);
                hint.style.left = leftpos + "px";
                hint.style.top =  toppos + "px";
                text = doc.createTextNode(hintCount + 1);
                hint.appendChild(text);

                hintContainer.appendChild(hint);
                hintCount++;
                hints.push([elem, hintCount, text, hint, elem.style.background, elem.style.color]);

                /* make the link black to ensure it's readable */
                elem.style.color = "#000";
                elem.style.background = "#ff0";
            }

            doc.documentElement.appendChild(hintContainer);

            /* recurse into any iframe or frame element */
            var frameTags = ["frame","iframe"];
            for (var i in frameTags) {
                var frames = doc.getElementsByTagName(frameTags[i]);
                for (var i = 0, nframes = frames.length; i < nframes; ++i) {
                    elem = frames[i];
                    rect = elem.getBoundingClientRect();
                    if (!elem.contentWindow || !rect || rect.left > maxX || rect.right < minX || rect.top > maxY || rect.bottom < minY)
                        continue;
                    helper(elem.contentWindow, offsetX + rect.left, offsetY + rect.top);
                }
            }
        }

        helper(topwin, 0, 0);

        this.clearFocus();
        this.focusHint(1);
        if (hintCount == 1) {
            /* just one hinted element - might as well follow it */
            return this.fire(1);
        }
    };

    /* set focus on hint with given number */
    this.focusHint = function(n)
    {
        /* reset previous focused hint */
        var hint = _getHintByNumber(currentFocusNum);
        if (hint !== null) {
            hint[0].className = hint[0].className.replace("hinting_mode_hint_focus", "hinting_mode_hint");
            hint[0].style.background = "#ff0";
        }

        currentFocusNum = n;

        /* mark new hint as focused */
        var hint = _getHintByNumber(currentFocusNum);
        if (hint !== null) {
            hint[0].className = hint[0].className.replace("hinting_mode_hint", "hinting_mode_hint_focus");
            hint[0].style.background = "#8f0";
        }
    };

    this.focusNextHint = function()
    {
        var index = _getHintIdByNumber(currentFocusNum);

        if (typeof(hints[index + 1]) != "undefined") {
            this.focusHint(hints[index + 1][1]);
        } else {
            this.focusHint(hints[0][1]);
        }
    };

    this.focusPreviousHint = function()
    {
        var index = _getHintIdByNumber(currentFocusNum);

        if (typeof(hints[index - 1][1]) != "undefined") {
            this.focusHint(hints[index - 1][1]);
        } else {
            this.focusHint(hints[hints.length - 1][1]);
        }
    };

    this.updateHints = function(n)
    {
        /* remove none matching hints */
        var remove = [];
        for (e in hints) {
            var hint = hints[e];
            if (0 != hint[1].toString().indexOf(n.toString())) {
                remove.push(hint[1]);
            }
        }

        for (var i = 0; i < remove.length; ++i) {
            _removeHint(remove[i]);
        }

        if (hints.length === 1) {
            return "fire;" + hints[0][1];
        }

        this.focusHint(n);
    };

    this.clearFocus = function()
    {
        if (document.activeElement && document.activeElement.blur) {
            document.activeElement.blur();
        }
    };

    this.clearHints = function()
    {
        for (e in hints) {
            var hint = hints[e];
            if (typeof(hint[0]) != "undefined") {
                hint[0].style.background = hint[4];
                hint[0].style.color = hint[5];
                hint[3].parentNode.removeChild(hint[3]);
            }
        }
        hintContainer.parentNode.removeChild(hintContainer);
        window.onkeyup = null;
    };

    this.fire = function(n)
    {
        var doc;
        if (!n) {
            var n = currentFocusNum;
        }
        var hint = _getHintByNumber(n);
        if (typeof(hint[0]) == "undefined")
            return;

        var el = hint[0];
        var tag = el.nodeName.toLowerCase();
        this.clearHints();

        if (tag == "iframe" || tag == "frame" || tag == "textarea" || tag == "input" && (el.type == "text" || el.type == "password" || el.type == "checkbox" || el.type == "radio") || tag == "select") {
            el.focus();
            return;
        }
        if (!el.onclick && el.href && !el.href.match("/^javascript:/")) {
            /* send signal to open link */
            return "open;" + el.href;
        }
        doc = el.ownerDocument;
        var evObj = doc.createEvent("MouseEvents");
        evObj.initMouseEvent("click", true, true, el.contentWindow, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, null);
        el.dispatchEvent(evObj);
    };

    this.focusInput = function()
    {
        if (document.getElementsByTagName("body")[0] === null || typeof(document.getElementsByTagName("body")[0]) != "object")
            return;

        /* prefixing html: will result in namespace error */
        var hinttags = "//input[@type='text'] | //input[@type='password'] | //textarea";
        var r = document.evaluate(hinttags, document,
            function(p) {
                return "http://www.w3.org/1999/xhtml";
            }, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
        var i;
        var j = 0;
        var k = 0;
        var first = null;
        for (i = 0; i < r.snapshotLength; i++) {
            var elem = r.snapshotItem(i);
            if (k == 0) {
                if (elem.style.display != "none" && elem.style.visibility != "hidden") {
                    first = elem;
                } else {
                    k--;
                }
            }
            if (j == 1 && elem.style.display != "none" && elem.style.visibility != "hidden") {
                elem.focus();
                break;
            }
            if (elem == document.activeElement) {
                j = 1;
            }
            k++;
        }
        /* no appropriate field found focused - focus the first one */
        if (j == 0 && first !== null)
            first.focus();
    };

    function _getTextFromElement(el)
    {
        if (el instanceof HTMLInputElement || el instanceof HTMLTextAreaElement) {
            text = el.value;
        } else if (el instanceof HTMLSelectElement) {
            if (el.selectedIndex >= 0) {
                text = el.item(el.selectedIndex).text;
            } else{
                text = "";
            }
        } else {
            text = el.textContent;
        }
        return text.toLowerCase();;
    }

    function _getHintByNumber(n)
    {
        var index = _getHintIdByNumber(n);
        if (index !== null) {
            return hints[index];
        }
        return null;
    }

    function _getHintIdByNumber(n)
    {
        for (var i = 0; i < hints.length; ++i) {
            var hint = hints[i];
            if (hint[1] === n) {
                return i;
            }
        }
        return null;
    }

    function _removeHint(n)
    {
        var index = _getHintIdByNumber(n);
        if (index === null) {
            return;
        }
        var hint = hints[index];
        if (hint[1] == n) {
            hint[0].style.background = hint[4];
            hint[0].style.color = hint[5];
            hint[3].parentNode.removeChild(hint[3]);

            /* remove hints from all hints */
            hints.splice(index, 1);
        }
    }
}
hints = new Hints();
