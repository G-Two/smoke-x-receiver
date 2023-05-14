<template>
  <div>
    <div id="nav">
      <h2>Smoke X Receiver</h2>
      <router-link to="/"> Status </router-link> |
      <router-link to="/wlan"> WLAN </router-link> |
      <router-link to="/pairing"> Pairing </router-link> |
      <router-link to="/mqtt"> MQTT </router-link>
    </div>
    <router-view />
  </div>
</template>

<style>
form {
  width: calc(100% - 2em);
  max-width: 480px;
  box-sizing: border-box;
  padding: 2em;
  box-shadow: 0 0 1em rgba(0, 0, 0, 0.1);
  border-radius: 0.5em;
  margin: 1em auto;
}

form pre {
  background-color: rgba(0, 100, 250, 0.1);
  padding: 1em;
}

h3 {
  display: block;
  min-width: 100%;
  width: 100%;
  text-align: center;
}

h4 {
  display: block;
  min-width: 100%;
  width: 100%;
  text-align: center;
}

#nav {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  color: #2c3e50;
  text-align: center;
  font-weight: bold;
  color: #2c3e50;
}

#nav a {
  font-weight: bold;
  color: #2c3e50;
  text-decoration: none;
}

#nav a.router-link-exact-active {
  color: #42b983;
}
</style>
