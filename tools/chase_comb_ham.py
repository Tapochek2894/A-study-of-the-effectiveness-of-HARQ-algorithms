import matplotlib.pyplot as plt
import pandas as pd

# Чтение данных
df = pd.read_csv('3.csv')

# Построение графика
plt.figure(figsize=(8, 5))
plt.plot(df['snr_db'], df['chase_combining'],
         'b-o', label='Chase combining', linewidth=2, markersize=5)
plt.plot(df['snr_db'], df['no_combining'],
         'r--s', label='Без комбинирования', linewidth=2, markersize=5)

plt.xlabel('SNR (dB)', fontsize=12)
plt.ylabel('Среднее число повторных передач', fontsize=12)
plt.title('Сравнение стратегий HARQ для кода Хэмминга (7,4)', fontsize=14)
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend(fontsize=11)
# plt.yscale('log')
plt.tight_layout()
plt.savefig('CHASE_NO_PASSBAND_7_4_1.png', dpi=150)
plt.show()