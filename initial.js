
window.onerror=h5gg_js_error_handler=function(message, url, line, column, error) {
    console.log('log---onerror::::', message, url, line, column, error);
    var isChinese = navigator.language.indexOf("zh-")==0;
    var fname=isChinese ? '(匿名脚本)' : "(anonymous code)";
    try {
        fname = decodeURI(new URL(url).pathname);
    } catch(e) {
        console.warn('Failed to parse error URL:', url, e);
    }
    if (isChinese)
        alert(`JS错误 在${fname}第${line}行第${column}列:\n\n${message}`);
    else
        alert(`JSError in:${fname} line:${line} column:${column}\n\n${message}`);
};

