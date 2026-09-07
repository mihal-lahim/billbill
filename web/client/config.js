export const CONFIG = {
  creator: 'ミハル',
  creatorSocialUrl: 'https://x.com/Mih4L_',
  feedbackUrl: 'https://forms.gle/P4EiRvd4opGdesM5A',
  websocketUrl: location.hostname === 'localhost'
    ? `ws://${location.hostname}:${location.port || 8080}/ws`
    : `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws`,
};
