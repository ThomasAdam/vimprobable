/*
    (c) 2009 by Leon Winter
    (c) 2009-2010 by Hannes Schueller
    (c) 2010 by Hans-Peter Deifel
    see LICENSE file
*/
function Hints() {
    var hintContainer;
    var hintElements;
    var hintCount;
    var focusedHint;
    var colors;
    var backgrounds;

    this.createHints = function (inputText)
    {
        if (document.getElementsByTagName("body")[0] !== null && typeof(document.getElementsByTagName("body")[0]) == "object") {
            var height = window.innerHeight;
            var width = window.innerWidth;
            var scrollX = document.defaultView.scrollX;
            var scrollY = document.defaultView.scrollY;
            this.genHintContainer();

            /* prefixing html: will result in namespace error */
            var hinttags;
            if (typeof(inputText) == "undefined" || inputText == "") {
                hinttags = "//*[@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href] | //input[not(@type='hidden')] | //a | //area | //iframe | //textarea | //button | //select";
            } else {
                /* only elements which match the text entered so far */
                hinttags = "//*[(@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href) and contains(., '" + inputText + "')] | //input[not(@type='hidden') and contains(., '" + inputText + "')] | //a[contains(., '" + inputText + "')] | //area[contains(., '" + inputText + "')] | //iframe[contains(@name, '" + inputText + "')] | //textarea[contains(., '" + inputText + "')] | //button[contains(@value, '" + inputText + "')] | //select[contains(., '" + inputText + "')]";
            }

            /*
            iterator type isn't suitable here, because: "DOMException NVALID_STATE_ERR:
            The document has been mutated since the result was returned."
            */
            var r = document.evaluate(hinttags, document,
                function(p) {
                    return 'http://www.w3.org/1999/xhtml';
                }, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

            /* due to the different XPath result type, we will need two counter variables */
            this.hintCount = 0;
            var i;
            this.hintElements = [];
            this.colors = [];
            this.backgrounds = [];
            for (i = 0; i < r.snapshotLength; i++)
            {
                var elem = r.snapshotItem(i);
                var rect = elem.getBoundingClientRect();
                if (!rect || rect.top > height || rect.bottom < 0 || rect.left > width || rect.right < 0 || !(elem.getClientRects()[0]))
                    continue;
                var computedStyle = document.defaultView.getComputedStyle(elem, null);
                if (computedStyle.getPropertyValue("visibility") != "visible" || computedStyle.getPropertyValue("display") == "none")
                    continue;
                var leftpos = Math.max((rect.left + scrollX), scrollX);
                var toppos = Math.max((rect.top + scrollY), scrollY);
                this.hintElements.push(elem);
                /* making this block DOM compliant */
                var hint = document.createElement("span");
                hint.setAttribute("class", "hinting_mode_hint");
                hint.setAttribute("id", "vimprobablehint" + this.hintCount);
                hint.style.position = "absolute";
                hint.style.left = leftpos + "px";
                hint.style.top =  toppos + "px";
                hint.style.background = "red";
                hint.style.color = "#fff";
                hint.style.font = "bold 10px monospace";
                hint.style.zIndex = "99";
                var text = document.createTextNode(this.hintCount + 1);
                hint.appendChild(text);
                this.hintContainer.appendChild(hint);
                /* remember site-defined colour of this element */
                this.colors[this.hintCount] = elem.style.color;
                this.backgrounds[this.hintCount] = elem.style.background;
                /* make the link black to ensure it's readable */
                elem.style.color = "#000";
                elem.style.background = "#ff0";
                this.hintCount++;
            }
            this.clearFocus();
            this.focusedHint = null;
            if (this.hintCount == 1) {
                /* just one hinted element - might as well follow it */
                return this.fire(1);
            }
        }
    };

    this.updateHints = function (n)
    {
        if(this.focusedHint != null) {
            this.focusedHint.className = this.focusedHint.className.replace("_focus","");
            this.focusedHint.style.background = "#ff0";
        }
        if (this.hintCount - 1 < n * 10 && typeof(this.hintElements[n - 1]) != "undefined") {
            /* return signal to follow the link */
            return "fire;" + n;
        } else {
            if (typeof(this.hintElements[n - 1]) != "undefined") {
                (this.focusedHint = this.hintElements[n - 1]).className = this.hintElements[n - 1].className.replace("hinting_mode_hint", "hinting_mode_hint_focus");
                this.focusedHint.style.background = "#8f0";
            }
        }
    };

    this.clearFocus = function ()
    {
        if (document.activeElement && document.activeElement.blur)
            document.activeElement.blur();
    };

    this.clearHints = function ()
    {
        for(e in this.hintElements) {
            if (typeof(this.hintElements[e].className) != "undefined") {
                this.hintElements[e].className = this.hintElements[e].className.replace(/hinting_mode_hint/,'');
                /* reset to site-defined colour */
                this.hintElements[e].style.color = this.colors[e];
                this.hintElements[e].style.background = this.backgrounds[e];
            }
        }
        this.hintContainer.parentNode.removeChild(this.hintContainer);
        window.onkeyup = null;
    };

    this.fire = function (n)
    {
        if (typeof(this.hintElements[n - 1]) != "undefined") {
            var el = this.hintElements[n - 1];
            var tag = el.nodeName.toLowerCase();
            this.clearHints();
            if(tag == "iframe" || tag == "frame" || tag == "textarea" || tag == "input" && (el.type == "text" || el.type == "password" || el.type == "checkbox" || el.type == "radio") || tag == "select") {
                el.focus();
                if (tag == "textarea" || tag == "input")
                    console.log('insertmode_on');
            } else {
                if (!el.onclick && el.href && !el.href.match('/^javascript:/')) {
                    /* send signal to open link */
                    return "open;" + el.href;
                } else {
                    var evObj = document.createEvent('MouseEvents');
                    evObj.initMouseEvent('click', true, true, window, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, null);
                    el.dispatchEvent(evObj);
                }
            }
        }
    };

    this.focusInput = function ()
    {
        if (document.getElementsByTagName("body")[0] !== null && typeof(document.getElementsByTagName("body")[0]) == "object") {
            /* prefixing html: will result in namespace error */
            var hinttags = "//input[@type='text'] | //input[@type='password'] | //textarea";
            var r = document.evaluate(hinttags, document,
                function(p) {
                    return 'http://www.w3.org/1999/xhtml';
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
                    var tag = elem.nodeName.toLowerCase();
                    if (tag == "textarea" || tag == "input")
                        console.log('insertmode_on');
                    break;
                } else {
                    if (elem == document.activeElement)
                        j = 1;
                }
                k++;
            }
            if (j == 0) {
                /* no appropriate field found focused - focus the first one */
                if (first !== null) {
                    first.focus();
                    var tag = elem.nodeName.toLowerCase();
                    if (tag == "textarea" || tag == "input")
                        console.log('insertmode_on');
                }
            }
        }
    };

    this.genHintContainer = function ()
    {
        var body = document.getElementsByTagName('body')[0];
        if (document.getElementById('hint_container'))
            return;

        this.hintContainer = document.createElement('div');
        this.hintContainer.id = "hint_container";

        if (body)
            body.appendChild(this.hintContainer);
    };
}
hints = new Hints();
