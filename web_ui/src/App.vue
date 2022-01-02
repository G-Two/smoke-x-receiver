<template>
  <div id="app">
    <div id="nav">
      <h2>Smoke X Receiver Config</h2>
      <router-link to="/wifi">Wifi</router-link> |
      <router-link to="/pairing">Pairing</router-link> |
      <router-link to="/mqtt">MQTT</router-link> |
      <router-link to="/lora">LoRa</router-link>
    </div>
    <div id="page">
    <router-view/>
    </div>
  </div>
</template>

<style>
#app {
  font-family: Avenir, Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  color: #2c3e50;
}

#page {
  background: #ffffff;
  margin: 2rem 0 4rem 0;
  padding-top: 2rem;
  position: relative;
  box-shadow: 0 2px 4px 0 rgba(0, 0, 0, 0.2), 0 2.5rem 5rem 0 rgba(0, 0, 0, 0.1);
}
@media screen and (min-width: 550px) {
#page {
    padding: 2rem;
  }
}

form {
  max-width: 62.5%;
  text-align: left;
  margin-bottom: 2rem;
}

.form-control {
  margin-bottom: 1rem;
}

#app h3 {
  display: block;
  min-width: 100%;
  width: 100%;
  text-align: center;
  margin: 0;
  margin-bottom: 1rem;
}

#app h4 {
  display: block;
  min-width: 100%;
  width: 100%;
  text-align: center;
  margin: 0;
  margin-bottom: 1rem;
}

#nav {
  padding-top: 1rem;
  margin: 0;
  margin-bottom: 2rem;
  text-align: center;
  font-weight: bold;
  color: #2c3e50;
}

#nav a {
  font-weight: bold;
  color: #2c3e50;
}

#nav a.router-link-exact-active {
  color: #42b983;
}
</style>
