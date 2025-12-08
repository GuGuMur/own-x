
import torch
from torch.utils.data import DataLoader
from torchvision import transforms
from torchvision.datasets import MNIST
import matplotlib.pyplot as plt

class Net(torch.nn.Module):
    def __init__(self):
        super(Net, self).__init__()
        self.fc1 = torch.nn.Linear(28 * 28, 64)
        self.fc2 = torch.nn.Linear(64, 64)
        self.fc3 = torch.nn.Linear(64, 64)
        self.fc4 = torch.nn.Linear(64, 10)

    def forward(self, x):
        x = torch.nn.functional.relu(self.fc1(x))
        x = torch.nn.functional.relu(self.fc2(x))
        x = torch.nn.functional.relu(self.fc3(x))
        x = self.fc4(x)
        return x
    

def get_data_loader(is_train: bool):
    transform = transforms.Compose([transforms.ToTensor()])
    dataset = MNIST(root='./data', train=is_train, download=True, transform=transform)
    return DataLoader(dataset, batch_size=15, shuffle=True)

def evaluate(test_data, net):
    correct = 0
    total = 0
    with torch.no_grad():
        for images, labels in test_data:
            outputs = net.forward(images.view(-1, 28 * 28))
            _, predicted = torch.max(outputs.data, 1)
            total += labels.size(0)
            correct += (predicted == labels).sum().item()
    return correct / total

def main():
    train_data = get_data_loader(is_train=True)
    test_data = get_data_loader(is_train=False)
    net = Net()

    print("initial accuracy:", evaluate(test_data, net))

    optimizer = torch.optim.SGD(net.parameters(), lr=0.01, momentum=0.9)
    loss_fn = torch.nn.CrossEntropyLoss()

    epochs = 2
    for epoch in range(epochs):
        for x, y in train_data:
            net.zero_grad()
            output = net.forward(x.view(-1, 28*28))
            loss = loss_fn(output, y)
            loss.backward()
            optimizer.step()
        print("epoch", epoch, "accuracy:", evaluate(test_data, net))
    for (n, (x, _)) in enumerate(test_data):
        if n > 3:
            break
        predict = torch.argmax(net.forward(x[0].view(-1, 28*28)))
        plt.figure(n)
        plt.imshow(x[0].view(28, 28))
        plt.title("prediction: " + str(int(predict)))
    plt.show()


if __name__ == "__main__":
    main()
