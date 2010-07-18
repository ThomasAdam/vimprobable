/*
    (c) 2009 by Leon Winter
    (c) 2009 by Hannes Schueller
    see LICENSE file
*/

function v(e, y) {
    t = e.nodeName.toLowerCase();
    if((t == 'input' && /^(text|password|checkbox|radio)$/.test(e.type))
    || /^(select|textarea)$/.test(t)
    || e.contentEditable == 'true')
        console.log('insertmode_'+(y=='focus'?'on':'off'));
}

if(document.activeElement)
    v(document.activeElement,'focus');

m=['focus','blur'];

if (document.getElementsByTagName("body")[0] !== null && typeof(document.getElementsByTagName("body")[0]) == "object") {
    for(i in m)
        document.getElementsByTagName('body')[0].addEventListener(m[i], function(x) {
            v(x.target,x.type);
        }, true);
}

self.onunload = function() {
    v(document.activeElement, '');
};
