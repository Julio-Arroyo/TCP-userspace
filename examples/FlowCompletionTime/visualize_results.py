import pandas as pd
import matplotlib.pyplot as plt


if __name__ == "__main__":
    df = pd.read_csv("results.csv")
    
    df0 = df[df.iloc[:,0] == 0]
    df001 = df[df.iloc[:,0] == 0.01]
    df01 = df[df.iloc[:,0] == 0.1]

    plt.plot(df0.iloc[:, 1].values, df0.iloc[:, 2].values, label='0%')
    plt.scatter(df0.iloc[:, 1].values, df0.iloc[:, 2].values)

    plt.plot(df001.iloc[:, 1].values, df001.iloc[:, 2].values, label='0.01%')
    plt.scatter(df001.iloc[:, 1].values, df001.iloc[:, 2].values)

    plt.plot(df01.iloc[:, 1].values, df01.iloc[:, 2].values, label='0.1%')
    plt.scatter(df01.iloc[:, 1].values, df01.iloc[:, 2].values)

    plt.xlabel("File size (bytes)")
    plt.ylabel("Flow completion time (ms)")
    plt.title("Time to transmit file w/ 10Mbps & 20ms delay at different loss rates")
    plt.legend()
    plt.savefig("results.png")

