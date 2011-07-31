/*
    (c) 2009 by Leon Winter
    (c) 2009, 2010 by Hannes Schueller
    (c) 2010 by Hans-Peter Deifel
    see LICENSE file
*/
function Hints() {
    var hintElement;
    var vimprobable_j;
    var vimprobable_a;
    var vimprobable_h;
    var vimprobable_colors;
    var vimprobable_backgrounds;

    this.createHints = function (inputText)
    {
        if (document.getElementsByTagName("body")[0] !== null && typeof(document.getElementsByTagName("body")[0]) == "object") {
            var height = window.innerHeight;
            var width = window.innerWidth;
            var scrollX = document.defaultView.scrollX;
            var scrollY = document.defaultView.scrollY;
            /* prefixing html: will result in namespace error */
            var hinttags;
            if (typeof(inputText) == "undefined" || inputText == "") {
                hinttags = "//*[@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href] | //input[not(@type='hidden')] | //a | //area | //iframe | //textarea | //button | //select";
            } else {
                /* only elements which match the text entered so far */
                hinttags = "//*[(@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href) and contains(., '" + inputText + "')] | //input[not(@type='hidden') and contains(., '" + inputText + "')] | //a[contains(., '" + inputText + "')] | //area[contains(., '" + inputText + "')] | //iframe[contains(@name, '" + inputText + "')] | //textarea[contains(., '" + inputText + "')] | //button[contains(@value, '" + inputText + "')] | //select[contains(., '" + inputText + "')]";
            }

            /* iterator type isn't suitable here, because: "DOMException NVALID_STATE_ERR: The document has been mutated since the result was returned." */
            var r = document.evaluate(hinttags, document,
                function(p) {
                    return 'http://www.w3.org/1999/xhtml';
                }, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
            this.hintElement = document.createElement("div");
            /* due to the different XPath result type, we will need two counter variables */
            this.vimprobable_j = 0;
            var i;
            this.vimprobable_a = [];
            this.vimprobable_colors = [];
            this.vimprobable_backgrounds = [];
            for (i = 0; i < r.snapshotLength; i++)
            {
                var elem = r.snapshotItem(i);
                rect = elem.getBoundingClientRect();
                if (!rect || rect.top > height || rect.bottom < 0 || rect.left > width || rect.right < 0 || !(elem.getClientRects()[0]))
                    continue;
                var computedStyle = document.defaultView.getComputedStyle(elem, null);
                if (computedStyle.getPropertyValue("visibility") != "visible" || computedStyle.getPropertyValue("display") == "none")
                    continue;
                var leftpos = Math.max((rect.left + scrollX), scrollX);
                var toppos = Math.max((rect.top + scrollY), scrollY);
                this.vimprobable_a.push(elem);
                /* making this block DOM compliant */
                var hint = document.createElement("span");
                hint.setAttribute("class", "hinting_mode_hint");
                hint.setAttribute("id", "vimprobablehint" + this.vimprobable_j);
                hint.style.position = "absolute";
                hint.style.left = leftpos + "px";
                hint.style.top =  toppos + "px";
                hint.style.background = "red";
                hint.style.color = "#fff";
                hint.style.font = "bold 10px monospace";
                hint.style.zIndex = "99";
                var text = document.createTextNode(this.vimprobable_j + 1);
                hint.appendChild(text);
                this.hintElement.appendChild(hint);
                /* remember site-defined colour of this element */
                this.vimprobable_colors[this.vimprobable_j] = elem.style.color;
                this.vimprobable_backgrounds[this.vimprobable_j] = elem.style.background;
                /* make the link black to ensure it's readable */
                elem.style.color = "#000";
                elem.style.background = "#ff0";
                this.vimprobable_j++;
            }
            i = 0;
            while (typeof(this.vimprobable_a[i]) != "undefined") {
                this.vimprobable_a[i].className += " hinting_mode_hint";
                i++;
            }
            document.getElementsByTagName("body")[0].appendChild(this.hintElement);
            this.clearFocus();
            this.vimprobable_h = null;
            if (i == 1) {
                /* just one hinted element - might as well follow it */
                return this.fire(1);
            }
        }
    };

    this.updateHints = function (n)
    {
        if(this.vimprobable_h != null) {
            this.vimprobable_h.className = this.vimprobable_h.className.replace("_focus","");
            this.vimprobable_h.style.background = "#ff0";
        }
        if (this.vimprobable_j - 1 < n * 10 && typeof(this.vimprobable_a[n - 1]) != "undefined") {
            /* return signal to follow the link */
            return "fire;" + n;
        } else {
            if (typeof(this.vimprobable_a[n - 1]) != "undefined") {
                (this.vimprobable_h = this.vimprobable_a[n - 1]).className = this.vimprobable_a[n - 1].className.replace("hinting_mode_hint", "hinting_mode_hint_focus");
                this.vimprobable_h.style.background = "#8f0";
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
        for(e in this.vimprobable_a) {
            if (typeof(this.vimprobable_a[e].className) != "undefined") {
                this.vimprobable_a[e].className = this.vimprobable_a[e].className.replace(/hinting_mode_hint/,'');
                /* reset to site-defined colour */
                this.vimprobable_a[e].style.color = this.vimprobable_colors[e];
                this.vimprobable_a[e].style.background = this.vimprobable_backgrounds[e];
            }
        }
        this.hintElement.parentNode.removeChild(this.hintElement);
        window.onkeyup = null;
    };
    
    this.fire = function (n)
    {
        if (typeof(this.vimprobable_a[n - 1]) != "undefined") {
            el = this.vimprobable_a[n - 1];
            tag = el.nodeName.toLowerCase();
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
}
hints = new Hints();
