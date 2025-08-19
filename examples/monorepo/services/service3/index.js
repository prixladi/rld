import { multiply } from './package1/index.js';
import { hello } from './package2/index.js';

(async () => {
  while (true) {
    console.log('---------------------------------------------');
    console.log('Service 3', multiply(5, 9), hello('Jerry'));
    console.log('---------------------------------------------');
    await new Promise((res) => setTimeout(res, 5 * 1000));
  }
})().catch((err) => console.error(err));
