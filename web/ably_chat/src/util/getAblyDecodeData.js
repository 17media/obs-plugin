import { ungzip } from 'pako/dist/pako_inflate.min';

const mapUnit8ArrayToString = array =>
    String.fromCodePoint.apply(null, array);

// After base64 decode, gzip needs to be ungzipped and converted to string
// Then processed through es5 escape, decodeURIComponent, and finally parsed into usable json data
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
                // Extract message data separately for decode processing, then merge back with fields other than data into new message
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
    // Since decodeCompressedData applies to both ably and pubnub, input will be formatted into the same format first
    const resultData = decodeCompressedData(msg);

    return resultData;
};