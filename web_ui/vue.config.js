const CompressionPlugin = require("compression-webpack-plugin");

module.exports = {
  filenameHashing: false,
  productionSourceMap: false,

  configureWebpack: {
    plugins: [
      new CompressionPlugin({
        minRatio: Infinity,
      }),
    ],
  },

  chainWebpack: (config) => {
    config.plugin("html").tap((args) => {
      args[0].title = "Smoke X Receiver";
      return args;
    });
  },
};
