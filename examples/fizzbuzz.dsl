namaste() {
  ginti i = 1;

  jabtak(i <= 15) {
    agar(i % 15 barabar 0) {
      likho("FizzBuzz");
    } warna {
      agar(i % 3 barabar 0) {
        likho("Fizz");
      } warna {
        agar(i % 5 barabar 0) {
          likho("Buzz");
        } warna {
          likho(i);
        }
      }
    }

    i = i + 1;
  }

  niklo(0);
}
