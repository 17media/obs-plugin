import { ungzip } from 'pako/dist/pako_inflate.min';

const mapUnit8ArrayToString = array =>
    String.fromCodePoint.apply(null, array);

// base64 decode 後的 gzip 還要 ungzip 並轉成 string 後
// 再經過 es5 escape、decodeURIComponent 處理，最後才 parse 成可用的 json data
export const decodeMessage = gzipMessage =>
    [escape, decodeURIComponent, JSON.parse].reduce(
        (message, fn) => fn(message),
        mapUnit8ArrayToString(ungzip(gzipMessage))
    );

export const decodeCompressedData = (
    rawMessage,
) => {
    if (!rawMessage.cdata) {
        return rawMessage;
    }

    let message = rawMessage;

    const { cdata, ...rest } = rawMessage;

    cdata.forEach(({ alg, data = '' }) => {
        if (alg === 'gzip_base64') {
            try {
                // 將 message data 單獨拉出來 decode 處理，再跟 data 以外欄位合併回新的 message
                message = {
                    ...rest,
                    ...decodeMessage(window.atob(data)),
                };
            } catch (e) {
                console.error(e.toString());
            }
        }
    });

    return message;
};

export const getAblyDecodeData = message => {
    const { data, ...rest } = message;
    const msg = {
        cdata: [
            {
                alg: 'gzip_base64',
                data,
            },
        ],
        ...rest,
    };
    // 因為 decodeCompressedData 同時適用於 ably 和 pubnub，input 會先被整成相同格式格式
    const resultData = decodeCompressedData(msg);

    return resultData;
};