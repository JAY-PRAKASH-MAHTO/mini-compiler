namaste() {
  likho("POORA DEMO SHURU");

  ginti total = 10;
  ginti step = 2;
  ginti product = total * step;
  ginti quotient = product / step;
  ginti remainder = quotient % 3;

  likho(product);
  likho(quotient);
  likho(remainder);

  agar(remainder > 0) {
    likho("REMAINDER HAI");
  } warna {
    likho("REMAINDER NAHI");
  }

  ginti i = 0;
  jabtak(i != 6) {
    agar(i == 0) {
      likho("SHURUATI STEP");
    } warna {
      chuno(i) {
        mamla 1:
          likho("JAARI MAMLA");
          i = i + 1;
          jaari;
        mamla 2:
          total = total - 3;
          likho(total);
          ruko;
        mamla 3:
          total = total + 5;
          likho(total);
          ruko;
        baki:
          agar(i < 4) {
            likho("CHAAR SE CHHOTA");
          } warna {
            agar(i > 4) {
              likho("CHAAR SE BADA");
            } warna {
              likho("BILKUL CHAAR");
            }
          }
          ruko;
      }
    }

    i = i + 1;
  }

  likho("POORA DEMO KHATAM");
  niklo(0);
}
