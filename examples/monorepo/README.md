## Rld monorepo example

Nodejs monorepo example of rld usage. It contains 3 services and 2 packages.

1) Service1 has no dependencies. To start it, run `rld run service1`
2) Service2 has dependency on package1. To start it, run `rld run service2`
3) Service3 has dependency on package1 and package2. To start it, run `rld run service3`

You can run all services at once, for example, in separate terminals. After that, you can start editing code in packages and services and observe changes. 